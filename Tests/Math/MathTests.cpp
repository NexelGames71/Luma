#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Luma/Math/Geometry.h"
#include "Luma/Math/Math.h"
#include "Luma/Math/MathUtils.h"
#include "Luma/Math/Quat.h"

using Catch::Approx;
using namespace Luma;        // f32, etc.
using namespace Luma::Math;  // Vec3, Quat, AABB, ...

namespace {
constexpr f32 kEps = 1e-4f;
bool Near(const Vec3& a, const Vec3& b) {
    return std::abs(a.x - b.x) < kEps && std::abs(a.y - b.y) < kEps &&
           std::abs(a.z - b.z) < kEps;
}
}  // namespace

// --- Utilities ---
TEST_CASE("Math utils clamp/lerp/length", "[math][utils]") {
    REQUIRE(Clamp(5.0f, 0.0f, 3.0f) == 3.0f);
    REQUIRE(Clamp(-1.0f, 0.0f, 3.0f) == 0.0f);
    REQUIRE(Clamp(2.0f, 0.0f, 3.0f) == 2.0f);
    REQUIRE(Lerp(0.0f, 10.0f, 0.25f) == Approx(2.5f));
    REQUIRE(Length(Vec3(3.0f, 4.0f, 0.0f)) == Approx(5.0f));
    REQUIRE(Distance(Vec3(1, 0, 0), Vec3(4, 4, 0)) == Approx(5.0f));
    REQUIRE(Degrees(kPi) == Approx(180.0f));
}

// --- Quaternion ---
TEST_CASE("Identity quaternion leaves vectors unchanged", "[math][quat]") {
    Quat q = Quat::Identity();
    REQUIRE(Near(q.Rotate(Vec3(1, 2, 3)), Vec3(1, 2, 3)));
}

TEST_CASE("Quaternion 90 deg about Y sends +X to -Z", "[math][quat]") {
    Quat q = Quat::FromAxisAngle(Vec3(0, 1, 0), Radians(90.0f));
    REQUIRE(Near(q.Rotate(Vec3(1, 0, 0)), Vec3(0, 0, -1)));
}

TEST_CASE("Quaternion ToMat4 matches RotateY for a Y rotation", "[math][quat]") {
    Quat q = Quat::FromAxisAngle(Vec3(0, 1, 0), Radians(90.0f));
    Mat4 qm = q.ToMat4();
    Mat4 rm = RotateY(Radians(90.0f));
    for (int i = 0; i < 16; ++i) {
        REQUIRE(qm.m[i] == Approx(rm.m[i]).margin(1e-4));
    }
}

TEST_CASE("Composing two Y-90 rotations gives a Y-180 rotation", "[math][quat]") {
    Quat q90 = Quat::FromAxisAngle(Vec3(0, 1, 0), Radians(90.0f));
    Quat q180 = q90 * q90;
    REQUIRE(Near(q180.Rotate(Vec3(1, 0, 0)), Vec3(-1, 0, 0)));
}

TEST_CASE("Normalize yields a unit quaternion", "[math][quat]") {
    Quat q{1.0f, 2.0f, 3.0f, 4.0f};
    Quat n = Normalize(q);
    REQUIRE(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z + n.w * n.w) ==
            Approx(1.0f));
}

TEST_CASE("Slerp endpoints return the inputs", "[math][quat]") {
    Quat a = Quat::Identity();
    Quat b = Quat::FromAxisAngle(Vec3(0, 1, 0), Radians(90.0f));
    REQUIRE(Near(Slerp(a, b, 0.0f).Rotate(Vec3(1, 0, 0)), Vec3(1, 0, 0)));
    REQUIRE(Near(Slerp(a, b, 1.0f).Rotate(Vec3(1, 0, 0)), Vec3(0, 0, -1)));
}

// --- AABB / Ray ---
TEST_CASE("AABB contains points and reports center/extents", "[math][aabb]") {
    AABB box{Vec3(-1, -1, -1), Vec3(1, 3, 1)};
    REQUIRE(box.Contains(Vec3(0, 0, 0)));
    REQUIRE_FALSE(box.Contains(Vec3(2, 0, 0)));
    REQUIRE(Near(box.Center(), Vec3(0, 1, 0)));
    REQUIRE(Near(box.Extents(), Vec3(1, 2, 1)));
}

TEST_CASE("AABB overlap test", "[math][aabb]") {
    AABB a{Vec3(0, 0, 0), Vec3(2, 2, 2)};
    AABB b{Vec3(1, 1, 1), Vec3(3, 3, 3)};
    AABB c{Vec3(3, 3, 3), Vec3(4, 4, 4)};
    REQUIRE(Overlaps(a, b));
    REQUIRE_FALSE(Overlaps(a, c));
}

TEST_CASE("Ray hits an AABB in front and misses one to the side",
          "[math][ray]") {
    AABB box{Vec3(-1, -1, -1), Vec3(1, 1, 1)};
    Ray hit{Vec3(0, 0, -5), Vec3(0, 0, 1)};
    Ray miss{Vec3(5, 0, -5), Vec3(0, 0, 1)};
    f32 t = 0.0f;
    REQUIRE(RayIntersectsAABB(hit, box, t));
    REQUIRE(t == Approx(4.0f));
    REQUIRE_FALSE(RayIntersectsAABB(miss, box, t));
}

TEST_CASE("Ray-sphere intersection returns the near hit distance",
          "[math][ray]") {
    Sphere s{Vec3(0, 0, 0), 1.0f};
    Ray r{Vec3(0, 0, -5), Vec3(0, 0, 1)};
    f32 t = 0.0f;
    REQUIRE(RayIntersectsSphere(r, s, t));
    REQUIRE(t == Approx(4.0f));
}

TEST_CASE("Ray-plane intersection finds the crossing", "[math][ray]") {
    Plane p{Vec3(0, 1, 0), 0.0f};  // y = 0 ground plane
    Ray r{Vec3(0, 5, 0), Normalize(Vec3(0, -1, 0))};
    f32 t = 0.0f;
    REQUIRE(RayIntersectsPlane(r, p, t));
    REQUIRE(t == Approx(5.0f));
}

// --- Frustum culling ---
TEST_CASE("Frustum from ortho clip accepts inside and rejects outside boxes",
          "[math][frustum]") {
    // Identity view, orthographic clip: box [-2,2] x [-1,1] x depth [0.1,10].
    Mat4 clip = Ortho(-2, 2, -1, 1, 0.1f, 10.0f);
    Frustum f = Frustum::FromViewProj(clip);

    auto boxAt = [](Vec3 c) {
        return AABB{c - Vec3(0.2f, 0.2f, 0.2f), c + Vec3(0.2f, 0.2f, 0.2f)};
    };
    REQUIRE(f.Intersects(boxAt(Vec3(0, 0, -1))));       // in front, centered
    REQUIRE_FALSE(f.Intersects(boxAt(Vec3(5, 0, -1))));  // off to the side
    REQUIRE_FALSE(f.Intersects(boxAt(Vec3(0, 0, -50)))); // beyond far plane
    REQUIRE_FALSE(f.Intersects(boxAt(Vec3(0, 0, 1))));   // behind the camera
}
