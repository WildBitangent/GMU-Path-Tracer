#pragma once

constexpr auto WIDTH = 1280u;
constexpr auto HEIGHT = 720u;

constexpr auto WIDTHF = static_cast<float>(WIDTH);
constexpr auto HEIGHTF = static_cast<float>(HEIGHT);

constexpr auto FOV = 60.f;

constexpr auto PATHCOUNT = 1 << 21; // 2M paths