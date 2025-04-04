/*
  Copyright (c) 2025 PICO Technology Co., Ltd. See LICENSE.md.
*/

#pragma once

#include <algorithm>
#include <cmath>

#include "HAL/Platform.h"
#include "Logging.h"
#include "Math/Float16.h"
#include "Math/FloatPacker.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "SplatConstants.h"

namespace PICO::Splat
{
namespace
{
/**
 * Converts a standard 32-bit float to a float with the given number of bits for
 * the exponent and significand, as well as the presence of a sign bit.
 * Supports any format with 32 or fewer bits.
 *
 * @param F - The float that will be converted.
 * @return The output, as a uint32, stored starting from the lowest bits.
 */
template <uint32 Signed, uint32 ExpBits, uint32 SigBits> uint32 ToFloat(float F)
{
	static_assert(Signed == 1 || Signed == 0);

	if (F == 0.f)
	{
		return 0;
	}

	/**
	 * HACK(seth): Attempt to remove after migration to UE 5.5+.
	 * This is a workaround for a bug in TFloatPacker. The exponent is coming
	 * back as 1 larger than expected, doubling all values.
	 */
	using FPacker = TFloatPacker<ExpBits, SigBits, false>;
	uint32 Raw = *reinterpret_cast<uint32*>(&F);

	int32 Exponent = (Raw & FFloatInfo_IEEE32::ExponentMask) >>
	                 FFloatInfo_IEEE32::MantissaBits;
	Exponent -= FFloatInfo_IEEE32::ExponentBias;
	Exponent = std::clamp<int32>(
		Exponent, -FPacker::ExponentBias + 1, FPacker::ExponentBias);
	Exponent += FPacker::ExponentBias;
	Exponent <<= SigBits;
	Exponent &= FPacker::ExponentMask;

	uint32 Significand = Raw & FFloatInfo_IEEE32::MantissaMask;
	Significand >>= FFloatInfo_IEEE32::MantissaBits - SigBits;
	Significand &= FPacker::MantissaMask;

	if constexpr (Signed == 1)
	{
		uint32 Sign = Raw & FFloatInfo_IEEE32::SignMask;
		Sign >>= FFloatInfo_IEEE32::SignShift - ExpBits - SigBits;
		Sign &= FPacker::SignMask;

		return Sign | Exponent | Significand;
	}
	else
	{
		return Exponent | Significand;
	}
}

/**
 * Converts a float to an unsigned, normalized integer, with the specified
 * number of bits.
 * Clips F to the range [0, 1].
 *
 * @param F - The float that will be converted.
 * @return An unsigned, normalized integer equivalent to F.
 */
template <uint32 Bits> uint32 ToUNorm(float F)
{
	static_assert(Bits <= 32);
	constexpr uint32 MaxValue = (1 << Bits) - 1;

	if (F == 0.f)
	{
		return 0;
	}
	else if (F > 1.f)
	{
		return MaxValue;
	}

	const float FScaled = std::round(F * MaxValue);
	return uint32(FScaled);
}
} // namespace

/**
 * Splat position, packed into 32-bits.
 *
 * The format is as follows:
 *   X: 11-bit unsigned normalized integer starting at bit 0.
 *   Y: 11-bit unsigned normalized integer starting at bit 11.
 *   Z: 10-bit unsigned normalized integer starting at bit 22.
 */
struct FPackedPos
{
	/**
	 * Creates a packed position at (0, 0, 0), relative to an external offset.
	 */
	FPackedPos() = default;

	/**
	 * Creates a packed position from normalized scalars.
	 *
	 * @param X - X, normalized between a min and max.
	 * @param Y - Y, normalized between a min and max.
	 * @param Z - Z, normalized between a min and max.
	 */
	FPackedPos(float X, float Y, float Z)
	{
		const uint32 XPacked = ToUNorm<11>(X);
		const uint32 YPacked = ToUNorm<11>(Y);
		const uint32 ZPacked = ToUNorm<10>(Z);

		Packed = (ZPacked << 22) | (YPacked << 11) | XPacked;
	}

	/**
	 * Creates a packed position from a vector, where each component is
	 * normalized between a per-axis minimum and maximum.
	 *
	 * @param V - Normalized vector to pack.
	 */
	FPackedPos(const FVector3f& V) : FPackedPos(V.X, V.Y, V.Z) {}

	/**
	 * Serializes / deserializes a packed position.
	 *
	 * @param Ar - Archive to load from or save to.
	 * @param P - Position to read or write.
	 */
	friend FArchive& operator<<(FArchive& Ar, FPackedPos& P)
	{
		return Ar << P.Packed;
	}

	static constexpr uint32 MAX_UNORM_10 = 0x3FF;
	static constexpr uint32 MAX_UNORM_11 = 0x7FF;
	// HACK(seth): FVector3f constructor with (X, Y, Z) isn't constexpr, so this
	// works around it.
	static constexpr FVector3f MAX = []() constexpr
	{
		FVector3f Max(MAX_UNORM_11, UE::Math::TVectorConstInit{});
		Max.Z = MAX_UNORM_10;
		return Max;
	}();

private:
	uint32 Packed;
};

/**
 * Splat covariance, packed into 64-bits.
 *
 * The format is as follows:
 *   XX: 10-bit unsigned float 5e5 starting at bit 54.
 *   XY: 11-bit   signed float 5e5 starting at bit 43.
 *   XZ: 11-bit   signed float 5e5 starting at bit 32.
 *   YY: 10-bit unsigned float 5e5 starting at bit 22.
 *   YZ: 11-bit   signed float 5e5 starting at bit 11.
 *   ZZ: 11-bit unsigned float 5e6 starting at bit 0.
 *
 * @see https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.html#10bitfp
 *
 * Note: As variances (XX, YY, ZZ, or the diagonal of the covariance matrix)
 * are the change on an axis with respect to itself, they will not be negative.
 * As such, we can save space by removing their sign bits. One bit is left over
 * in this scenario, which here is given to ZZ for its significand.
 */
struct FPackedCovMat
{
	/**
	 * Creates packed covariances matrix of all zeros, *not* an identity matrix.
	 */
	FPackedCovMat() = default;

	/**
	 * Creates packed covariance matrix from upper-triangular portion of given
	 * matrix.
	 *
	 * @param Sigma - Covariance matrix. Only upper-triangular portion will be
	 * read.
	 */
	FPackedCovMat(const FMatrix44f& Sigma)
	{
		// Following B10G11R11 layout, roughly.
		const uint64 XXPacked = ToFloat<0, 5, 5>(Sigma.M[0][0]);
		const uint64 XYPacked = ToFloat<1, 5, 5>(Sigma.M[0][1]);
		const uint64 XZPacked = ToFloat<1, 5, 5>(Sigma.M[0][2]);
		const uint32 YYPacked = ToFloat<0, 5, 5>(Sigma.M[1][1]);
		const uint32 YZPacked = ToFloat<1, 5, 5>(Sigma.M[1][2]);
		const uint32 ZZPacked = ToFloat<0, 5, 6>(Sigma.M[2][2]);

		Packed = (XXPacked << 54) | (XYPacked << 43) | (XZPacked << 32) |
		         (YYPacked << 22) | (YZPacked << 11) | ZZPacked;
	}

	/**
	 * Serializes / deserializes a packed covariance matrix.
	 *
	 * @param Ar - Archive to load from or save to.
	 * @param M - Matrix to read or write.
	 */
	friend FArchive& operator<<(FArchive& Ar, FPackedCovMat& M)
	{
		return Ar << M.Packed;
	}

private:
	uint64 Packed;
};

/**
 * Convenience type for holding an (Index, Distance) pair.
 * This is used for CPU sorting, where it is more performant to keep the indices
 * and distances together (rather than in two separate buffers).
 */
struct FIndexedDistance
{
	/**
	 * Creates a new FIndexedDistance for a given splat, relative to a view.
	 *
	 * @param InIndex - Index of the splat this measures.
	 * @param OriginCM - Origin of the view, in centimeters.
	 * @param Forward - Direction of the view, normalized.
	 * @param PositionCM - Position of the splat, in centimeters.
	 */
	FIndexedDistance(
		uint32 InIndex,
		const FVector3f& OriginCM,
		const FVector3f& Forward,
		const FVector3f& PositionCM)
		: Index(InIndex)
	{
		/**
		 * By default, Unreal places the near plane at 10cm and the far
		 * plane at infinity. The depth buffer is inverted, putting the near
		 * plane at 1 in NDC, and the far plane at 0.
		 * The equation for this is as follows:
		 *
		 * Z_NDC = 10 / Z_View = Z_Clip / W_Clip
		 *
		 * This gives a good distribution of precision to the depth buffer,
		 * so we might as well follow the same formula prior to quantizing.
		 * To be clear, it isn't necessary for splats to be sorted by their
		 * Z-buffer values, as any order-preserving depth value would work. We just
		 * need to make sure we are sorting by view-space depth (the distance to the
		 * near plane), *not* the distance to the view origin.
		 *
		 * @see https://mathworld.wolfram.com/Point-PlaneDistance.html, eq. 13.
		 */

		check(Forward.IsNormalized());

		FVector3f DeltaPositionCM = PositionCM - OriginCM;
		float Z = DeltaPositionCM.Dot(Forward);

		Distance = (Z >= NEAR_CLIP_CM) ? uint16(NEAR_CLIP_CM / Z * MAX_DISTANCE)
		                               : NOT_VISIBLE;
	}

	/**
	 * Returns whether this splat is nearer than the provided one. Used to
	 * support std::sort.
	 */
	bool operator<(const FIndexedDistance& ID) const
	{
		return Distance < ID.Distance;
	}

	/**
	 * Returns whether the splat at the associated index is in front of the
	 * viewer. This is static to work with std::partition, in order to avoid
	 * sorting splats we can guarantee aren't visible.
	 *
	 * @param ID - The (Index, Distance) pair to check for visibility.
	 * @return If the splat is possibly visible.
	 */
	static bool IsMaybeVisible(const FIndexedDistance& ID)
	{
		return ID.Distance != NOT_VISIBLE;
	}

private:
	// TODO(seth): These should tie into SplatConstants.h, SplatSettings.h and shaders.
	static constexpr uint16 MAX_DISTANCE = 0xFFFE;
	static constexpr float NEAR_CLIP_CM = 10.f;
	static constexpr uint16 NOT_VISIBLE = 0xFFFF;
	uint32 Index;
	uint16 Distance;
};

// Safety check.
static_assert(sizeof(FIndexedDistance) == 8);

} // namespace PICO::Splat
