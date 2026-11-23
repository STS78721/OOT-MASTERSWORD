// Luminance-preserving, pixel-local error redistribution in linear display light.
// The error coefficients are rows of I-S from Machado et al.'s severity-1 Rec.709 simulation.
// The colour returned is corrected: move the error into a visible opponent axis and compress
// chroma toward neutral if it would leave the display gamut. Luminance and neutral greys stay fixed.
const vec3 cv_y709 = vec3(0.2126, 0.7152, 0.0722);
const vec3 cv_y2020 = vec3(0.2627, 0.6780, 0.0593);
const mat3 cv_to2020 = mat3(0.627404, 0.069097, 0.016392,
                           0.329283, 0.919540, 0.088013,
                           0.043313, 0.011362, 0.895595);
const mat3 cv_to709 = mat3(1.660491, -0.124550, -0.018151,
                          -0.587641, 1.132900, -0.100579,
                          -0.072850, -0.008349, 1.118730);
bool cv_enabled(int mode) { return mode >= 1 && mode <= 3; }

vec3 cv_decode_srgb(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), greaterThan(c, vec3(0.04045)));
}
vec3 cv_encode_srgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, greaterThan(c, vec3(0.0031308)));
}
vec3 cv_correct(vec3 rgb, int mode, bool wide, float limit) {
    if (!cv_enabled(mode)) return rgb;
    const vec3 luma = wide ? cv_y2020 : cv_y709;
    const vec3 c = wide ? cv_to709 * rgb : rgb;
    const vec3 error_row = mode == 1 ? vec3(0.847714, 0.0, 0.204868)
                                    : mode == 2 ? vec3(0.632678, 0.0, 0.227968)
                                                : vec3(-0.004733, 0.0, 0.696100);
    // Subtract green explicitly so the rounded matrix coefficients preserve neutral colours.
    const float error = dot(c - c.g, error_row);
    const vec3 axis = mode == 3 ? vec3(1.0, -cv_y709.r / cv_y709.g, 0.0)
                               : vec3(-cv_y709.b / (1.0 - cv_y709.b), -cv_y709.b / (1.0 - cv_y709.b), 1.0);
    const vec3 shift = wide ? cv_to2020 * axis : axis;
    const vec3 corrected = rgb + 0.75 * error * (shift - dot(shift, luma));
    const float y = dot(rgb, luma);
    const vec3 chroma = corrected - y;
    const float low = min(chroma.r, min(chroma.g, chroma.b));
    const float high = max(chroma.r, max(chroma.g, chroma.b));
    const float scale = min(1.0, min(y / max(-low, 1e-9), (limit - y) / max(high, 1e-9)));
    return clamp(y + max(scale, 0.0) * chroma, 0.0, limit);
}
