#pragma once

// This is just starting resolution, and it may change during execution
constexpr auto WIDTH = 1280u; 
constexpr auto HEIGHT = 720u;

constexpr auto WIDTHF = static_cast<float>(WIDTH);
constexpr auto HEIGHTF = static_cast<float>(HEIGHT);

constexpr auto FOV = 60.f;

constexpr auto NUM_SM = 34;
constexpr auto PATHCOUNT = 1 << 21; // 2M paths
constexpr auto NUM_GROUPS = NUM_SM * 8;
constexpr auto MAX_LIGHTS = 128;
constexpr auto NUM_THREADS = 256;
constexpr auto ITERATIONS = PATHCOUNT / (NUM_GROUPS * NUM_THREADS);

constexpr auto CAPTURE_DIR_NAME = R"(Captures)";
constexpr auto CAPTURE_NAME = "potato";

constexpr auto DEFAULT_SCENE = "bunny_glass\\scene.gltf";
