#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <atomic>
#include <condition_variable>
#include <limits>
#include <math.h>
#include <mutex>
#include <queue>
#include <utility>
#include <unordered_set>
#include <vector>

#define let auto

constexpr bool is_little_endian() {
	let x = 1;
	return *(char*)&x;
}

template<typename T> T byte_swap(T t) {
	let* p = (uint8_t*)&t;
	for(std::size_t i = 0; i < sizeof(t) / 2; i++) {
		std::swap(p[i], p[sizeof(t) - 1 - i]);
	}
	return t;
}

// https://stackoverflow.com/questions/2353211/hsl-to-rgb-color-conversion
/**
 * Converts an HSL color value to RGB. Conversion formula
 * adapted from http://en.wikipedia.org/wiki/HSL_color_space.
 * Assumes s and l are contained in the set [0, 1] and h is [0, 360]
 * returns r, g, and b in the set [0, 255].
 *
 * @param   {number}  h       The hue
 * @param   {number}  s       The saturation
 * @param   {number}  l       The lightness
 * @return  {Array}           The RGB representation
 */
inline float hue2rgb(float p, float q, float t){
	if(t < 0) t += 1;
	if(t > 1) t -= 1;
	if(t < 1/6.) return p + (q - p) * 6 * t;
	if(t < 1/2.) return q;
	if(t < 2/3.) return p + (q - p) * (2/3. - t) * 6;
	return p;
}
inline std::tuple<uint8_t, uint8_t, uint8_t> hsl_to_rgb(float h, float s, float l){
	h /= 360;
	float r, g, b;
	if(s == 0) {
		r = g = b = l; // achromatic
	} else {
		float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
		float p = 2 * l - q;
		r = hue2rgb(p, q, h + 1/3.);
		g = hue2rgb(p, q, h);
		b = hue2rgb(p, q, h - 1/3.);
	}
	return {round(r * 255), round(g * 255), round(b * 255)};
}

// ceiling division
template<typename T> T cdiv(T x, T y) {
	return (x + y - 1) / y;
}

template<typename A, typename B, typename T> auto lerp(A a, B b, T t) {
	return a + t * (b - a);
}

#endif
