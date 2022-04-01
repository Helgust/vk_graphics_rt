#include "blue_noise_gen.h"

// random float number in range [0,1]
float NextFloat() {
	return  static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
}

// distance between two 2D points
float DistanceSquared(float x1, float y1, float x2, float y2) {
	return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
}

float DistanceSamples(const std::vector<float>& p, const std::vector<float>& q) {
	const int d = p.size();
	float res = 0.0f;
	for (size_t i = 0; i < p.size(); ++i) {
		res += std::pow(std::abs(p[i] - q[i]), (d / 2.0));
	}
	return res;
}


void BlueNoiseGenerator::init()
{
	for (size_t i = 0; i < x_resolution * y_resolution; ++i) {
		for (size_t d = 0; d < depth; ++d) {
			float _t = NextFloat();
			blue_noise[i].push_back(_t);
		}
	}
}

void BlueNoiseGenerator::getNoise(std::vector<uint32_t> &noiseMap, size_t width, size_t height)
{
    noiseMap.resize(height * width);
    
    for (int x = 0; x < width; ++x) {
		for (int y = 0; y < height; ++y) {
			std::vector<float> v = blue_noise[y * width + x];
			noiseMap[y * width + x] = 255 * v[0];
		}
	}
}

float BlueNoiseGenerator::E()
{
	float loss = 0;
	const float sigma_i = 2.1f;
	const float sigma_s = 1.0f;

//#pragma omp parallel for num_threads(threads) reduction(+: loss) // use openmp 
for (int px = 0; px < x_resolution; ++px) {
    for (int py = 0; py < y_resolution; ++py) {
        int i = px * x_resolution + py;
        std::vector<float> ps = blue_noise[i];

        float pix = i / x_resolution, piy = i % x_resolution;

        if (kernel_size == -1) {
            // use all pixel pairs to compute the loss
            for (int qx = 0; qx < x_resolution; ++qx) {
                for (int qy = 0; qy < y_resolution; ++qy) {
                    float qix = qx, qiy = qy;

                    float pixel_distance = DistanceSquared(pix, piy, qix, qiy);

                    int qqx = (qx + x_resolution) % x_resolution;
                    int qqy = (qy + y_resolution) % y_resolution;

                    int j = qqx * x_resolution + qqy;
                    if (j == i) continue;
                    std::vector<float> qs = blue_noise[j];

                    float sample_distance = DistanceSamples(ps, qs);
                    loss += std::exp(
                        -(pixel_distance) / (sigma_i * sigma_i)
                        - (sample_distance) / (sigma_s * sigma_s));
                }
            }
        }
        else {
            // use all neighborhood around the pixel (px,py) to approximate the loss
            for (int qx = pix - kernel_size; qx <= pix + kernel_size; ++qx) {
                for (int qy = piy - kernel_size; qy <= piy + kernel_size; ++qy) {
                    float qix = qx, qiy = qy;

                    float pixel_distance = DistanceSquared(pix, piy, qix, qiy);

                    int qqx = (qx + x_resolution) % x_resolution;
                    int qqy = (qy + y_resolution) % y_resolution;

                    int j = qqx * x_resolution + qqy;
                    if (j == i) continue;
                    std::vector<float> qs = blue_noise[j];

                    float sample_distance = DistanceSamples(ps, qs);
                    loss += std::exp(
                        -(pixel_distance) / (sigma_i * sigma_i)
                        - (sample_distance) / (sigma_s * sigma_s));
                }
            }
        }
    }
}
	return loss;
}

void BlueNoiseGenerator::optimize(int max_iter, bool verbose)
{

	// 2. random swap two pixels to minimize the loss
	float loss = E();
	int size = x_resolution * y_resolution;

	for (auto _i = 0; _i != max_iter; _i++) 
    {
		int i = NextFloat() * (size - 1);
		int j = NextFloat() * (size - 1);
		if (i == j) continue;

		std::swap(blue_noise[i], blue_noise[j]);

		float new_loss = E();

		// swap back.
		if (new_loss > loss) {
			std::swap(blue_noise[i], blue_noise[j]);
		}
		else {
			loss = new_loss;
		}
	}
}
