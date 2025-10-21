#include <sys/inotify.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>

void parse_json() {
    printf("config.json changed! Parsing...\n");
    // TODO: Read and parse config.json here
}

void watch_config_file() {
    int fd, wd;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t size;
    time_t last_parse_time = 0;
    
    fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }
    
    wd = inotify_add_watch(fd, ".", IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE);
    if (wd == -1) {
        fprintf(stderr, "Cannot watch directory: %s\n", strerror(errno));
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    last_parse_time = time(NULL);
    
    while (1) {
        size = read(fd, buf, sizeof(buf));
        
        if (size == -1 && errno != EAGAIN) {
            perror("read");
            break;
        }
        
        if (size <= 0) {
            usleep(100000);
            continue;
        }
        
        bool should_parse = false;
        
        // Process all events
        for (char *ptr = buf; ptr < buf + size;
             ptr += sizeof(struct inotify_event) + event->len) {
            
            event = (const struct inotify_event *)ptr;
            
            if (event->len && strcmp(event->name, "config.json") == 0) {
                if (event->mask & (IN_CLOSE_WRITE | IN_MOVED_TO | IN_CREATE)) {
                    should_parse = true;
                }
            }
        }
        
        // Only parse if we haven't parsed in the last second (debounce)
        if (should_parse) {
            time_t now = time(NULL);
            if (now - last_parse_time >= 1) {
                parse_json();
                last_parse_time = now;
            }
        }
    }
    
    close(fd);
}

int main() {
    watch_config_file();
    return 0;
}
