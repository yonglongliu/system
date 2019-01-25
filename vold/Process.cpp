/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/stat.h>
#include <signal.h>

#define LOG_TAG "ProcessKiller"
#include <cutils/log.h>

#include "Process.h"

int Process::readSymLink(const char *path, char *link, size_t max) {
    struct stat s;
    int length;

    if (lstat(path, &s) < 0)
        return 0;
    if ((s.st_mode & S_IFMT) != S_IFLNK)
        return 0;
   
    // we have a symlink    
    length = readlink(path, link, max- 1);
    if (length <= 0) 
        return 0;
    link[length] = 0;
    return 1;
}

int Process::pathMatchesMountPoint(const char* path, const char* mountPoint) {
    int length = strlen(mountPoint);
    if (length > 1 && strncmp(path, mountPoint, length) == 0) {
        // we need to do extra checking if mountPoint does not end in a '/'
        if (mountPoint[length - 1] == '/')
            return 1;
        // if mountPoint does not have a trailing slash, we need to make sure
        // there is one in the path to avoid partial matches.
        return (path[length] == 0 || path[length] == '/');
    }
    
    return 0;
}

/* SPRD: Add for boot performance in cryptfs mode {@  */
extern "C" void vold_getProcessName(int pid, char *buffer, size_t max) {
   Process::getProcessName(pid, buffer, max);
}
/* @} */

void Process::getProcessName(int pid, char *buffer, size_t max) {
    int fd;
    snprintf(buffer, max, "/proc/%d/cmdline", pid);
    fd = open(buffer, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        strcpy(buffer, "???");
    } else {
        int length = read(fd, buffer, max - 1);
        buffer[length] = 0;
        close(fd);
    }
}

int Process::checkFileDescriptorSymLinks(int pid, const char *mountPoint) {
    return checkFileDescriptorSymLinks(pid, mountPoint, NULL, 0);
}

/* SPRD: Add for boot performance in cryptfs mode {@ */
extern "C" int vold_checkFileDescriptorSymLinks(int pid, const char *mountPoint, char *openFilename, size_t max) {
    return Process::checkFileDescriptorSymLinks(pid, mountPoint, openFilename, max);
}

extern "C" int vold_checkSymLink(int pid, const char *mountPoint, const char *name) {
    return Process::checkSymLink(pid, mountPoint, name);
}

/* @} */

int Process::checkFileDescriptorSymLinks(int pid, const char *mountPoint, char *openFilename, size_t max) {


    // compute path to process's directory of open files
    char    path[PATH_MAX];
    sprintf(path, "/proc/%d/fd", pid);
    DIR *dir = opendir(path);
    if (!dir)
        return 0;

    // remember length of the path
    int parent_length = strlen(path);
    // append a trailing '/'
    path[parent_length++] = '/';

    struct dirent* de;
    while ((de = readdir(dir))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")
                || strlen(de->d_name) + parent_length + 1 >= PATH_MAX)
            continue;
        
        // append the file name, after truncating to parent directory
        path[parent_length] = 0;
        strcat(path, de->d_name);

        char link[PATH_MAX];

        if (readSymLink(path, link, sizeof(link)) && pathMatchesMountPoint(link, mountPoint)) {
            if (openFilename) {
                memset(openFilename, 0, max);
                strlcpy(openFilename, link, max);
            }
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

int Process::checkFileMaps(int pid, const char *mountPoint) {
    return checkFileMaps(pid, mountPoint, NULL, 0);
}

int Process::checkFileMaps(int pid, const char *mountPoint, char *openFilename, size_t max) {
    FILE *file;
    char buffer[PATH_MAX + 100];

    sprintf(buffer, "/proc/%d/maps", pid);
    file = fopen(buffer, "r");
    if (!file)
        return 0;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        // skip to the path
        const char* path = strchr(buffer, '/');
        if (path && pathMatchesMountPoint(path, mountPoint)) {
            if (openFilename) {
                memset(openFilename, 0, max);
                strlcpy(openFilename, path, max);
            }
            fclose(file);
            return 1;
        }
    }
    
    fclose(file);
    return 0;
}

int Process::checkSymLink(int pid, const char *mountPoint, const char *name) {
    char    path[PATH_MAX];
    char    link[PATH_MAX];

    sprintf(path, "/proc/%d/%s", pid, name);
    if (readSymLink(path, link, sizeof(link)) && pathMatchesMountPoint(link, mountPoint)) 
        return 1;
    return 0;
}

/* SPRD: Add for boot performance in cryptfs mode {@ */
extern "C" int vold_getPid(const char *s) {
    return Process::getPid(s);
}
/* @} */

int Process::getPid(const char *s) {
    int result = 0;
    while (*s) {
        if (!isdigit(*s)) return -1;
        result = 10 * result + (*s++ - '0');
    }
    return result;
}

extern "C" void vold_killProcessesWithOpenFiles(const char *path, int signal) {
	Process::killProcessesWithOpenFiles(path, signal);
}

/*
 * Hunt down processes that have files open at the given mount point.
 */
// SPRD: support double sdcard change return type from "void" to "bool"
bool Process::killProcessesWithOpenFiles(const char *path, int signal) {
    DIR*    dir;
    struct dirent* de;
    /* SPRD: support double sdcard do not kill system_server process with signal SIGTERM@{ */
    bool value = true;
    /* @} */

    if (!(dir = opendir("/proc"))) {
        SLOGE("opendir failed (%s)", strerror(errno));
        return value; // SPRD: support double sdcard return line add "value"
    }

    while ((de = readdir(dir))) {
        int pid = getPid(de->d_name);
        char name[PATH_MAX];

        if (pid == -1)
            continue;
        getProcessName(pid, name, sizeof(name));

        char openfile[PATH_MAX];

        if (checkFileDescriptorSymLinks(pid, path, openfile, sizeof(openfile))) {
            SLOGE("Process %s (%d) has open file %s", name, pid, openfile);
        } else if (checkFileMaps(pid, path, openfile, sizeof(openfile))) {
            SLOGE("Process %s (%d) has open filemap for %s", name, pid, openfile);
        } else if (checkSymLink(pid, path, "cwd")) {
            SLOGE("Process %s (%d) has cwd within %s", name, pid, path);
        } else if (checkSymLink(pid, path, "root")) {
            SLOGE("Process %s (%d) has chroot within %s", name, pid, path);
        } else if (checkSymLink(pid, path, "exe")) {
            SLOGE("Process %s (%d) has executable path within %s", name, pid, path);
        } else {
            continue;
        }
        /* SPRD: support double sdcard kill slog process force when unmount sd card for performance @{ */
        if(!strcmp(name, "/system/bin/slog") || !strcmp(name, "/system/bin/slogmodem")){
            SLOGE("Sending SIGKILL to %s process %d", name , pid);
            kill(pid, SIGKILL);
            continue;
        }
        /* @} */
        if (signal != 0) {
          /* SPRD: support double sdcard do not kill system_server process with signal SIGTERM @{ */
          if (!strcmp(name, "system_server")) {
              SLOGW("Process name is system_server, do not kill \n");
              if(signal == SIGKILL) value = false;
              continue;
          }else {
              SLOGW("vold:Sending %s to process %s (%d)", strsignal(signal), name, pid);
                /* @} */
              SLOGW("Sending %s to process %d", strsignal(signal), pid);
              kill(pid, signal);
          /* SPRD: support double sdcard do not kill system_server process with signal SIGTERM @{ */
           }
           /* @} */
        }
    }
    closedir(dir);
    // SPRD: support double sdcard add return "value"
    return value;
}
