#pragma once

// Native box-art variants authored for the 1080p layout. Dimensions are maximum bounds; each
// image keeps its original aspect ratio and is never enlarged while preparing a variant.
enum class ArtworkVariant { Full, Home, Grid, Small, Detail, Arcade };

struct ArtworkBounds {
    int width;
    int height;
    const char *suffix;
};

inline ArtworkBounds artworkBounds(ArtworkVariant variant) {
    switch (variant) {
    case ArtworkVariant::Home:   return {178, 178, "-home"};
    case ArtworkVariant::Grid:   return {245, 245, "-grid"};
    case ArtworkVariant::Small:  return {128, 128, "-small"};
    case ArtworkVariant::Detail: return {689, 624, "-detail"};
    case ArtworkVariant::Arcade: return {148, 148, "-arcade"};
    case ArtworkVariant::Full:
    default:                     return {0, 0, ""};
    }
}
