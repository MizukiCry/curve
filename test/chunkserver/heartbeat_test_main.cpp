/*
 * Copyright (C) 2018 NetEase Inc. All rights reserved.
 * Project: Curve
 *
 * History:
 *          2018/12/23  Wenyu Zhou   Initial version
 */

#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "include/chunkserver/chunkserver_common.h"
#include "src/chunkserver/chunkserver.h"
#include "src/chunkserver/heartbeat.h"
#include "test/chunkserver/heartbeat_test.h"

static char* param[3][2] = {
    {
        "-minloglevel=2",
        "-conf=deploy/local/chunkserver/conf/chunkserver.conf.0",
    },
    {
        "-minloglevel=2",
        "-conf=deploy/local/chunkserver/conf/chunkserver.conf.1",
    },
    {
        "-minloglevel=2",
        "-conf=deploy/local/chunkserver/conf/chunkserver.conf.2",
    },
};

using ::curve::chunkserver::ChunkServer;

butil::AtExitManager atExitManager;

static int RunChunkServer(int i, int argc, char** argv) {
    auto chunkserver = new curve::chunkserver::ChunkServer();
    if (chunkserver == nullptr) {
        LOG(ERROR) << "Failed to create chunkserver " << i;
        return -1;
    }
    int ret = chunkserver->Run(argc, argv);
    if (ret < 0) {
        LOG(ERROR) << "Failed to run chunkserver process " << ret;
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    int ret;
    pid_t pids[3];

    testing::InitGoogleTest(&argc, argv);

    LOG_IF(ERROR, 0 != curve::chunkserver::RemovePeersData(true))
        << "Failed to remove peers' data";

    pids[0] = fork();
    if (pids[0] < 0) {
        LOG(FATAL) << "Failed to create chunkserver process 0";
    } else if (pids[0] == 0) {
        return RunChunkServer(0, sizeof(param[0])/sizeof(char *), param[0]);
    }

    pids[1] = fork();
    if (pids[1] < 0) {
        LOG(FATAL) << "Failed to create chunkserver process 1";
    } else if (pids[1] == 0) {
        return RunChunkServer(1, sizeof(param[1])/sizeof(char *), param[1]);
    }

    pids[2] = fork();
    if (pids[2] < 0) {
        LOG(FATAL) << "Failed to create chunkserver process 2";
    } else if (pids[2] == 0) {
        return RunChunkServer(2, sizeof(param[2])/sizeof(char *), param[2]);
    }

    // main test process
    {
        LOG(INFO) << "Run all test...";
        pid_t pid = fork();
        if (pid < 0) {
            LOG(FATAL) << "Failed to create test proccess";
        } else if (pid == 0) {
            ret = RUN_ALL_TESTS();
            return ret;
        }
        waitpid(pid, &ret, 0);
        LOG(INFO) << "Run all test end... Return code: " << ret;

        LOG(INFO) << "Stop all chunkserver";
        for (int i = 0; i < 3; i++) {
            kill(pids[i], SIGINT);
            waitpid(pids[i], &ret, 0);
        }
        LOG(INFO) << "Stop all chunkserver success";

        LOG(INFO) << "Restart chunkserver 1";
        pid = fork();
        if (pid < 0) {
            LOG(FATAL) << "Failed to restart chunkserver process 1";
        } else if (pid == 0) {
            ret = RunChunkServer(1, sizeof(param[1])/sizeof(char *), param[1]);
            return ret;
        }
        sleep(2);
        kill(pid, SIGINT);
        waitpid(pid, &ret, 0);
        LOG(INFO) << "Stop restart chunkserver 1 ok. Return code: " << ret;

        LOG(INFO) << "Heartbeat testing finished.";
        return ret;
    }
}