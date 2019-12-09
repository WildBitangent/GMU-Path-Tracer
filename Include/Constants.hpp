#pragma once

// This is just starting resolution, and it may change during execution
constexpr auto WIDTH = 1280u; 
constexpr auto HEIGHT = 720u;

constexpr auto WIDTHF = static_cast<float>(WIDTH);
constexpr auto HEIGHTF = static_cast<float>(HEIGHT);

constexpr auto FOV = 60.f;

constexpr auto PATHCOUNT = 1 << 21; // 2M paths

constexpr auto CAPTURE_DIR_NAME = R"(Captures)";
constexpr auto CAPTURE_NAME = "potato";

constexpr auto DEFAULT_SCENE = "bunny_glass\\scene.gltf";

constexpr auto MAX_LIGHTS = 128u;