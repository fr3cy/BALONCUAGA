#pragma once
#ifdef clamp
#undef clamp
#endif
namespace HudResolution {
// Fixed pixel sizing. Screen dimensions are used only for positioning/clipping.
static constexpr float factor(int,int){return 1.f;}
// Constant font quality; changing resolution must not rebuild the font atlas.
static constexpr int density(int,int){return 3;}
static int segments(float radius){return std::clamp(static_cast<int>(std::ceil(6.2831853f*std::max(1.f,radius)/3)),32,1024);}
}
