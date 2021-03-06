/*
 * Copyright 2019 Xilinx, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef HLS_TEST
#include "xcl2.hpp"
#endif
#include <cstring>
#include <vector>
#include <fstream>
#include <iostream>
#include <sys/time.h>
#include "ap_int.h"
#include "utils.hpp"
#include "inflation_capfloor_engine_kernel.hpp"
#define XCL_BANK(n) (((unsigned int)(n)) | XCL_MEM_TOPOLOGY)

#define XCL_BANK0 XCL_BANK(0)
#define XCL_BANK1 XCL_BANK(1)
#define XCL_BANK2 XCL_BANK(2)
#define XCL_BANK3 XCL_BANK(3)
#define XCL_BANK4 XCL_BANK(4)
#define XCL_BANK5 XCL_BANK(5)
#define XCL_BANK6 XCL_BANK(6)
#define XCL_BANK7 XCL_BANK(7)
#define XCL_BANK8 XCL_BANK(8)
#define XCL_BANK9 XCL_BANK(9)
#define XCL_BANK10 XCL_BANK(10)
#define XCL_BANK11 XCL_BANK(11)
#define XCL_BANK12 XCL_BANK(12)
#define XCL_BANK13 XCL_BANK(13)
#define XCL_BANK14 XCL_BANK(14)
#define XCL_BANK15 XCL_BANK(15)

class ArgParser {
   public:
    ArgParser(int& argc, const char** argv) {
        for (int i = 1; i < argc; ++i) mTokens.push_back(std::string(argv[i]));
    }
    bool getCmdOption(const std::string option, std::string& value) const {
        std::vector<std::string>::const_iterator itr;
        itr = std::find(this->mTokens.begin(), this->mTokens.end(), option);
        if (itr != this->mTokens.end() && ++itr != this->mTokens.end()) {
            value = *itr;
            return true;
        }
        return false;
    }

   private:
    std::vector<std::string> mTokens;
};

int main(int argc, const char* argv[]) {
    std::cout << "\n----------------------YoY Inflation Black CapFloor Engine-----------------\n";
    // cmd parser
    ArgParser parser(argc, argv);
    std::string xclbin_path;
    if (!parser.getCmdOption("-xclbin", xclbin_path)) {
        std::cout << "ERROR:xclbin path is not set!\n";
        return 1;
    }
    // Allocate Memory in Host Memory
    double* times_alloc = aligned_alloc<double>(LEN);
    double* rates_alloc = aligned_alloc<double>(LEN);
    double* cfRate_alloc = aligned_alloc<double>(2);
    DT* output = aligned_alloc<DT>(1);

    // -------------setup k0 params---------------
    int err = 0;
    DT minErr = 10e-10;

    double golden = 44127.176256570005;
    int type = 0;
    DT forwardRate = 0.05;
    DT cfRate[2] = {0.025, 0.01};
    DT nomial = 1000000;
    DT gearing = 1.0;
    DT accrualTime = 1.0;
    int size = 16;
    DT time[16] = {-0.20000000000000001, 0.80000000000000004, 1.8,
                   2.7999999999999998,   3.7999999999999998,  4.7999999999999998,
                   5.7999999999999998,   6.7999999999999998,  7.7999999999999998,
                   8.8000000000000007,   9.8000000000000007,  11.800000000000001,
                   14.800000000000001,   19.800000000000001,  24.800000000000001,
                   29.800000000000001};
    DT rate[16] = {0.029500000000000002, 0.029500000000000082, 0.029500000000000082, 0.028868711597109832,
                   0.030279261405510949, 0.02910148264089029,  0.032179481772810818, 0.031893390809901619,
                   0.032498157697050056, 0.032566891097167636, 0.033080553534265505, 0.033100103081736429,
                   0.033057636349131544, 0.032260604369703967, 0.029523797449523093, 0.032844712457090405};
    int optionlets = 10;

    for (int i = 0; i < size; i++) {
        times_alloc[i] = time[i];
    }

    for (int i = 0; i < size; i++) {
        rates_alloc[i] = rate[i];
    }

    for (int i = 0; i < 2; i++) {
        cfRate_alloc[i] = cfRate[i];
    }

#ifndef HLS_TEST
    // do pre-process on CPU
    struct timeval start_time, end_time, test_time;
    // platform related operations
    std::vector<cl::Device> devices = xcl::get_xil_devices();
    cl::Device device = devices[0];

    // Creating Context and Command Queue for selected Device
    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    std::string devName = device.getInfo<CL_DEVICE_NAME>();
    printf("Found Device=%s\n", devName.c_str());

    // cl::Program::Binaries xclBins = xcl::import_binary_file("../xclbin/MCAE_u250_hw.xclbin");
    cl::Program::Binaries xclBins = xcl::import_binary_file(xclbin_path);
    devices.resize(1);
    cl::Program program(context, devices, xclBins);
    cl::Kernel kernel_InflationEngine(program, "INFLATION_k0");
    std::cout << "kernel has been created" << std::endl;

    cl_mem_ext_ptr_t mext_o[4];
    mext_o[0].obj = output;
    mext_o[0].param = 0;

    mext_o[1].obj = times_alloc;
    mext_o[1].param = 0;

    mext_o[2].obj = rates_alloc;
    mext_o[2].param = 0;

    mext_o[3].obj = cfRate_alloc;
    mext_o[3].param = 0;

    for (int i = 0; i < 4; ++i) {
#ifndef USB_HBM
        mext_o[i].flags = XCL_MEM_DDR_BANK0;
#else
        mext_o[i].flags = XCL_BANK0;
#endif
    }

    // create device buffer and map dev buf to host buf
    cl::Buffer output_buf;
    cl::Buffer times_buf, rates_buf, cfRate_buf;
    output_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, sizeof(DT) * N,
                            &mext_o[0]);
    times_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, sizeof(DT) * LEN,
                           &mext_o[1]);
    rates_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, sizeof(DT) * LEN,
                           &mext_o[2]);
    cfRate_buf = cl::Buffer(context, CL_MEM_EXT_PTR_XILINX | CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, sizeof(DT) * 2,
                            &mext_o[3]);

    std::vector<cl::Memory> ob_out;
    ob_out.push_back(output_buf);

    q.finish();
    // launch kernel and calculate kernel execution time
    std::cout << "kernel start------" << std::endl;
    gettimeofday(&start_time, 0);
    kernel_InflationEngine.setArg(0, type);
    kernel_InflationEngine.setArg(1, forwardRate);
    kernel_InflationEngine.setArg(2, cfRate_buf);
    kernel_InflationEngine.setArg(3, nomial);
    kernel_InflationEngine.setArg(4, gearing);
    kernel_InflationEngine.setArg(5, accrualTime);
    kernel_InflationEngine.setArg(6, size);
    kernel_InflationEngine.setArg(7, times_buf);
    kernel_InflationEngine.setArg(8, rates_buf);
    kernel_InflationEngine.setArg(9, optionlets);
    kernel_InflationEngine.setArg(10, output_buf);

    int loop_num = 1;
    for (int i = 0; i < loop_num; ++i) {
        q.enqueueTask(kernel_InflationEngine, nullptr, nullptr);
    }

    q.finish();
    gettimeofday(&end_time, 0);
    std::cout << "kernel end------" << std::endl;
    std::cout << "Execution time " << tvdiff(&start_time, &end_time) << "us" << std::endl;
    q.enqueueMigrateMemObjects(ob_out, 1, nullptr, nullptr);
    q.finish();
#else
    INFLATION_k0(type, forwardRate, cfRate, nomial, gearing, accrualTime, size, time, rate, optionlets, output);
#endif
    DT out = output[0];
    if (std::fabs(out - golden) > minErr) err++;
    std::cout << "NPV= " << out << " ,diff/NPV= " << (out - golden) / golden << std::endl;
    return err;
}
