#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Luma/Math/Math.h"
#include "Luma/Scene/Components.h"
#include "Luma/Scene/Scene.h"
#include "Luma/Scene/SceneSerializer.h"

using Catch::Approx;
using Luma::CameraComponent;
using Luma::NameComponent;
using Luma::ProjectionType;
using Luma::Scene;
using Luma::SceneSerializer;

TEST_CASE("CameraComponent perspective projection matches Math::Perspective",
          "[scene][camera]") {
    CameraComponent cam;  // defaults: perspective, 60deg, 0.1..1000
    Luma::Math::Mat4 p = cam.ProjectionMatrix(1.6f);
    Luma::Math::Mat4 ref =
        Luma::Math::Perspective(Luma::Math::Radians(60.0f), 1.6f, 0.1f, 1000.0f);
    for (int i = 0; i < 16; ++i) {
        REQUIRE(p.m[i] == Approx(ref.m[i]).margin(1e-4));
    }
}

TEST_CASE("CameraComponent orthographic projection uses height and aspect",
          "[scene][camera]") {
    CameraComponent cam;
    cam.projection = ProjectionType::Orthographic;
    cam.orthoHeight = 10.0f;
    Luma::Math::Mat4 p = cam.ProjectionMatrix(2.0f);  // w=10, h=5
    Luma::Math::Mat4 ref = Luma::Math::Ortho(-10.0f, 10.0f, -5.0f, 5.0f,
                                             cam.nearZ, cam.farZ);
    for (int i = 0; i < 16; ++i) {
        REQUIRE(p.m[i] == Approx(ref.m[i]).margin(1e-4));
    }
}

TEST_CASE("CameraComponent round-trips through scene serialization",
          "[scene][camera]") {
    Scene scene;
    Luma::Entity e = scene.CreateEntity("Main Camera");
    auto& cam = scene.Registry().emplace<CameraComponent>(e);
    cam.projection = ProjectionType::Orthographic;
    cam.fovYDegrees = 50.0f;
    cam.orthoHeight = 7.5f;
    cam.nearZ = 0.5f;
    cam.farZ = 250.0f;
    cam.primary = false;

    const std::string json = SceneSerializer::SaveToString(scene);
    REQUIRE(json.find("Camera") != std::string::npos);

    Scene loaded;
    REQUIRE(SceneSerializer::LoadFromString(loaded, json));

    bool found = false;
    auto view = loaded.Registry().view<NameComponent>();
    for (auto ent : view) {
        if (view.get<NameComponent>(ent).name == "Main Camera") {
            found = true;
            REQUIRE(loaded.Registry().all_of<CameraComponent>(ent));
            const auto& c = loaded.Registry().get<CameraComponent>(ent);
            REQUIRE(c.projection == ProjectionType::Orthographic);
            REQUIRE(c.fovYDegrees == Approx(50.0f));
            REQUIRE(c.orthoHeight == Approx(7.5f));
            REQUIRE(c.nearZ == Approx(0.5f));
            REQUIRE(c.farZ == Approx(250.0f));
            REQUIRE(c.primary == false);
        }
    }
    REQUIRE(found);
}
