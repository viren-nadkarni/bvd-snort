#include <iostream>
#include <cmath>
#include <vector>

#include "cl.hpp"

/*
std::vector<long> factorise(double);

std::vector<long> factorise(double n) {
    long z = 2;
    std::vector<long> factors;

    while(z * z <= n ) {
        if (std::fmod(n, z) == 0) {
            factors.push_back(z);
            n = n / z;
        } else {
            ++z;
        }
    }

    if(n > 1) {
        factors.push_back(n);
    }

    return factors;
}
*/
struct X {
    int number;
    int array[10];
};

int main(int argc, char* argv[]) {
    /*
    for(auto i: factorise(atol(argv[1])))
        std::cout << i << ' ';

    */

    std::vector<cl::Platform> all_platforms;

    cl::Platform::get(&all_platforms);

    if(all_platforms.size() == 0) {
        std::cout << "No platforms detected" << std::endl;
        exit(1);
    }

    cl::Platform default_platform = all_platforms[0];
    std::cout << "Using platform: " << default_platform.getInfo<CL_PLATFORM_NAME>()
        << std::endl;

    std::vector<cl::Device> all_devices;

    default_platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);

    if(all_devices.size() == 0) {
        std::cout << "No platforms detected" << std::endl;
        exit(1);
    }

    cl::Device default_device = all_devices[0];
    std::cout << "Using device: " << default_device.getInfo<CL_DEVICE_NAME>()
        << std::endl;

    cl::Context context;
    context = cl::Context({default_device});

    cl::Program::Sources sources;

    std::string kernel_code = R"SIMONGOBACK(
    struct X {
        int number;
        int array[10];
    };

    void kernel meaning_of_life(global struct X* x, global int* resp) {
        *resp = x[0].array[4];
    }
    )SIMONGOBACK";

    sources.push_back({kernel_code.c_str(), kernel_code.length()});

    cl::Program program(context, sources);

    if(program.build({default_device}) != CL_SUCCESS) {
        std::cout << "Error building: "
            << program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(default_device)
            << std::endl;
        exit(1);
    }

    int A[10] = {0};
    X x[2] = {{0, {0, 0, 0, 0, 42, 0, 0, 0, 0, 0}},
              {0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};
    int resp;

    std::cout << "Magic: " << x[0].array[4] << std::endl;

    cl::Buffer buffer_A(context, CL_MEM_READ_WRITE, sizeof(X)*2);
    cl::Buffer buffer_B(context, CL_MEM_READ_WRITE, sizeof(int));

    cl::CommandQueue queue(context, default_device);

    queue.enqueueWriteBuffer(buffer_A, CL_TRUE, 0, sizeof(X)*2, x);
    queue.enqueueWriteBuffer(buffer_B, CL_TRUE, 0, sizeof(int), &resp);

    cl::NDRange global(10);
    cl::NDRange local(1);

    cl::Kernel kernel = cl::Kernel(program, "meaning_of_life");
    kernel.setArg(0, buffer_A);
    kernel.setArg(1, buffer_B);

    queue.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            global,
            local
    );

    //queue.finish();

    int Z[10] = {0};

    queue.enqueueReadBuffer(buffer_B, CL_TRUE, 0, sizeof(int), &resp);

    std::cout << "Meaning of life:\n";

    /*
    for(auto i: Z)
        std::cout << i << ' ';
        */
    std::cout << resp << std::endl;

    return 0;
}
