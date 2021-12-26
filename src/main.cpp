#include <algorithm>
#include <assert.h>
#include <atomic>
#include <complex>
#include <random>
#include <stdint.h>
#include <stdio.h>
#include <thread>
#include <vector>

#include "bmp.h"

typedef double fp;

// image parameters
constexpr int w = 2560 * 2;
constexpr int h = 1440 * 2;
constexpr fp xmin = -2.5;
constexpr fp xmax = 1;
constexpr fp ymin = -1;
constexpr fp ymax = 1;
constexpr fp dx = (xmax - xmin) / w;
constexpr fp dy = (ymax - ymin) / h;

// render parameters
constexpr bool zebra = true;
constexpr size_t iterations = 1000;
constexpr size_t categorizer = 100; // 200;

// anti-aliasing settings
constexpr bool AA = true;
constexpr int AA_samples = 20;
thread_local std::mt19937 rng;
std::uniform_real_distribution<fp> ux(-dx/2, dx/2);
std::uniform_real_distribution<fp> uy(-dy/2, dy/2);

struct point_descriptor {
	bool escaped;
	size_t escape_time;
	fp infinum;
	std::complex<fp> escape_point;
};

point_descriptor mandelbrot(fp x, fp y) {
	std::complex<fp> c = std::complex<fp>(x, y);
	std::complex<fp> z = c; // z starts at 0, first iteration = c
	fp infinum2 = std::norm(z);
	size_t i;
	for(i = 1; i < iterations && std::norm(z) < 4; i++) {
		z = z * z + c;
		if(let norm = std::norm(z); norm < infinum2) {
			infinum2 = norm;
		}
	}
	return { std::norm(z) > 4, i, std::sqrt(infinum2), z };
}

// So there's some interesting optimization stuff going on here.
// This logic is pulled out because I haven't wanted -ffast-math effecting this computation.
// Previously the function returned std::tuple<fp, fp>.
// But it turns out [[gnu::optimize("-fno-fast-math")]] or [[gnu::optimize("-O3")]] resulted in
// really bad codegen here when -ffast-math was provided on the command line (fine codegen
// with just -O3 and not -ffast-math) because the call to the std::tuple constructor could not be
// inlined as it did not have [[gnu::optimize("-fno-fast-math")]]. I find this cool but there's also
// no way I know of to deal with it other than creating a type that won't have a constructor to
// inline. https://godbolt.org/z/PffTGjbaY
// Todo: Though it's not a big deal, this function can't be inlined into its callsites because of
// the compiler trying to maintaining ffast-math consistency, is there a way to allow it to be? Will
// it be done during LTO?
struct not_a_tuple { fp i, j; };
[[gnu::optimize("-fno-fast-math")]]// don't want ffast-math messing with this particular computation
not_a_tuple get_coordinates(int i, int j) {
	return {xmin + ((fp)i / w) * (xmax - xmin), ymin + ((fp)j / h) * (ymax - ymin)};
}

int color_escape(size_t escape_time) {
	if(escape_time <= 10) {
		return 255;
	} else {
		return 255 - std::min((escape_time - 10) * 3, (size_t)255);
	}
}

pixel_t get_pixel(fp x, fp y) {
	if(let result = mandelbrot(x, y); !result.escaped) {
		if(zebra) {
			int category = result.infinum * categorizer;
			return category % 2 == 0 ? 255 : 0;
		} else {
			// for experimentally figuring out the maximum infinum:
			//static thread_local fp max = 0;
			//if(result.infinum > max) {
			//	max = result.infinum;
			//	fprintf(stderr, "-> %f\n", max);
			//}
			constexpr fp max_infinum = 0.355; // highest observed was 0.354244
			//return std::min(result.infinum / max_infinum, (fp)1) * 255;
			return std::min(std::pow(result.infinum / max_infinum, 2), (fp)1) * 255;
			//return std::min(std::pow(result.infinum / max_infinum, .5), (fp)1) * 255;
		}
	} else {
		fp log_zn = std::log(std::norm(result.escape_point)) / 2;
		fp nu = std::log(log_zn / std::log(2)) / std::log(2);
		return (int)lerp(color_escape(result.escape_time), color_escape(result.escape_time + 1), 1 - nu);
	}
}

pixel_t sample(fp x, fp y) {
	if(AA) {
		int r = 0, g = 0, b = 0;
		for(int i = 0; i < AA_samples; i++) {
			let color = get_pixel(x + ux(rng), y + uy(rng));
			r += color.r;
			g += color.g;
			b += color.b;
		}
		return {(uint8_t)((fp)r/AA_samples), (uint8_t)((fp)g/AA_samples), (uint8_t)((fp)b/AA_samples)};
	} else {
		return get_pixel(x, y);
	}
}

void brute_force_worker(std::atomic_int* xj, BMP* bmp, int id) {
	int j;
	while((j = xj->fetch_add(1, std::memory_order_relaxed)) < h) {
		if(id == 0) printf("\033[1K\r%0.2f%%", (fp)j / h * 100);
		if(id == 0) fflush(stdout);
		for(int i = 0; i < w; i++) {
			let [x, y] = get_coordinates(i, j);
			let color = sample(x, y);
			bmp->set(i, j, color);
		}
	}
}

int main() {
	assert(byte_swap(0x11223344) == 0x44332211);
	assert(byte_swap(pixel_t{0x11, 0x22, 0x33}) == (pixel_t{0x33, 0x22, 0x11}));
	BMP bmp = BMP(w, h);
	puts("starting brute force");
	const int nthreads = std::thread::hardware_concurrency();
	std::vector<std::thread> vec(nthreads);
	std::atomic_int j = 0;
	for(int i = 0; i < nthreads; i++) {
		vec[i] = std::thread(brute_force_worker, &j, &bmp, i);
	}
	for(let& t : vec) {
		t.join();
	}
	puts("\033[1K\rfinished");
	bmp.write("test.bmp");
}
