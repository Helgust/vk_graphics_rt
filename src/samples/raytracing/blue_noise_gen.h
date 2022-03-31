#include <vector>
#include <numeric>
#include <cmath>
#include <cstdlib>

class BlueNoiseGenerator 
{
public:
	
	BlueNoiseGenerator() {};

	BlueNoiseGenerator(int x_resolution, int y_resolution, int depth = 1, int kernel_size = -1) 
	:x_resolution(x_resolution), y_resolution(y_resolution), depth(depth), kernel_size(kernel_size)
	{
	blue_noise.resize(x_resolution * y_resolution);
	init();
	};


	void optimize(int max_iter) {
		return optimize(max_iter, false);
	}
	
	void optimize(int max_iter, bool verbose);

    void getNoise(std::vector<uint32_t> &noiseMap, size_t width, size_t height);

	float E();

private:
	std::vector<std::vector<float>> blue_noise;
	int x_resolution=0, y_resolution=0, depth=1;
	int kernel_size = -1;
	int threads = 4;
	void init();
	
};