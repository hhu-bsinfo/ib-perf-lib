/*
 * Copyright (C) 2018 Heinrich-Heine-Universitaet Duesseldorf,
 * Institute of Computer Science, Department Operating Systems
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <dirent.h>
#include <cstring>
#include <csignal>
#include <iostream>
#include <chrono>
#include <thread>
#include <verbs.h>
#include <detector/BuildConfig.h>
#include <detector/IbDiagPerfCounter.h>
#include <detector/exception/IbFileException.h>
#include <vector>

bool isRunning = true;

static void SignalHandler(int signal) {
    if(signal == SIGINT) {
        isRunning = false;
    }
}

int main(int argc, char *argv[]) {
    Detector::BuildConfig::printBanner();

    int32_t numDevices;
    std::vector<Detector::IbDiagPerfCounter *> counters;

    ibv_device **deviceList = ibv_get_device_list(&numDevices);

    if(deviceList == nullptr) {
        printf("Unable to get device list! Error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for(int32_t i = 0; i < numDevices; i++) {
        const char *deviceName = ibv_get_device_name(deviceList[i]);
        ibv_context *deviceContext = ibv_open_device(deviceList[i]);

        if(deviceContext == nullptr) {
            printf("Unable to open device context!\n");
            exit(EXIT_FAILURE);
        }

        ibv_device_attr deviceAttributes{};
        int ret = ibv_query_device(deviceContext, &deviceAttributes);

        if(ret != 0) {
            printf("Unable to query device attributes from '%s'! Error: %s\n", deviceName, strerror(ret));
            exit(EXIT_FAILURE);
        }

        counters.emplace_back(new Detector::IbDiagPerfCounter(deviceName, 0));

        for(uint8_t j = 1; j < deviceAttributes.phys_port_cnt + 1; j++) {
            counters.emplace_back(new Detector::IbDiagPerfCounter(deviceName, j));
        }

        ibv_close_device(deviceContext);
    }

    ibv_free_device_list(deviceList);

    signal(SIGINT, SignalHandler);

    while (isRunning) {
        try {
            for(const auto &diagCounter : counters) {
                diagCounter->RefreshCounters();
                std::cout << *diagCounter << std::endl << std::endl;
            }

            std::cout << std::endl;

        } catch (const Detector::IbFileException &exception) {
            printf("An exception occurred: %s\n", exception.what());
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    for(const auto &diagCounter : counters) {
        delete diagCounter;
    }
}