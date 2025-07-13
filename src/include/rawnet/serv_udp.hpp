#pragma once

#include <stdexcept>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "common.hpp"

namespace nw {
    struct message {
        address client_addr;
        std::vector<uint8_t> data;
    };

    class server_udp {
            bool stop_event = false;
            std::function<void(address, std::vector<uint8_t>&, std::vector<uint8_t>&)> v_on_msg_cli;

            int buf_len;
            int threads;
            address addr;

            int sockfd;
            struct sockaddr_in servaddr;
            
            // Message queue for thread-safe processing
            std::queue<message> msg_queue;
            std::mutex queue_mutex;
            std::condition_variable queue_cv;
            
            std::thread receiver_thread;
            std::vector<std::thread> worker_threads;

            void receiver_loop() {
                while (!stop_event) {
                    struct sockaddr clisock;
                    socklen_t len = sizeof(clisock);
    
                    std::vector<uint8_t> buffer;
                    buffer.resize(buf_len);
                    
                    int n = recvfrom(
                        sockfd, &buffer[0], buf_len,  
                        MSG_WAITALL, 
                        (struct sockaddr *) &clisock, &len
                    );
                    
                    if (n < 0) {
                        if(stop_event) break;
                        else continue;
                    }

                    if (n >= 0) {
                        buffer.resize(n);
                    } else {
                        buffer.clear();
                    }
                    
                    struct sockaddr_in* cli_in_addr = (struct sockaddr_in*) &clisock;
                    address cli_addr(
                        inet_ntoa(cli_in_addr->sin_addr),
                        ntohs(cli_in_addr->sin_port)
                    );
                    
                    // Add message to queue
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        msg_queue.push({cli_addr, buffer});
                    }
                    queue_cv.notify_one();
                }
            }

            void worker_loop() {
                while (!stop_event) {
                    message msg;
                    bool got_message = false;
                    
                    // Quickly grab a message from queue with minimal locking
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        if (queue_cv.wait_for(lock, std::chrono::milliseconds(100), 
                            [this] { return !msg_queue.empty() || stop_event; })) {
                            
                            if (stop_event) break;
                            
                            if (!msg_queue.empty()) {
                                msg = msg_queue.front();
                                msg_queue.pop();
                                got_message = true;
                            }
                        }
                    } // Lock is released here
                    
                    if (!got_message) {
                        continue;
                    }
                    
                    // Process message without holding any locks
                    std::vector<uint8_t> ans_buffer;
                    v_on_msg_cli(msg.client_addr, msg.data, ans_buffer);
                    
                    // Send response
                    struct sockaddr_in cli_sockaddr;
                    cli_sockaddr.sin_family = AF_INET;
                    cli_sockaddr.sin_addr.s_addr = inet_addr(msg.client_addr.ip.c_str());
                    cli_sockaddr.sin_port = htons(msg.client_addr.port);
                    
                    sendto(
                        sockfd, 
                        &ans_buffer[0], ans_buffer.size(), 
                        MSG_CONFIRM, 
                        (const struct sockaddr *) &cli_sockaddr, sizeof(cli_sockaddr)
                    );
                }
            }

        public:
            address get_addr(){ return addr; }
            int get_threads_num(){ return threads; }

            void on_msg_cli (
                std::function<void(address, std::vector<uint8_t>&, std::vector<uint8_t>&)> v
            ){ v_on_msg_cli = v; }

            void set_threads(int num){ threads = num; }
            
            void run(bool detached = false, bool clear_buffer = true, int max_buff_size = 4096){
                buf_len = max_buff_size;
                
                // Start receiver thread
                receiver_thread = std::thread([this]() { receiver_loop(); });
                
                // Start worker threads
                worker_threads.resize(threads);
                for (int i = 0; i < threads; i++) {
                    worker_threads[i] = std::thread([this]() { worker_loop(); });
                }

                if (!detached) {
                    receiver_thread.join();
                    for (auto& t : worker_threads) {
                        t.join();
                    }
                } else {
                    receiver_thread.detach();
                    for (auto& t : worker_threads) {
                        t.detach();
                    }
                }
            }

            void create(address addr){
                if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
                    throw std::runtime_error("[r-e] server socket creation failed");
                }

                int opt = 1;
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
                    close(sockfd);
                    throw std::runtime_error("[r-e] server setsockopt(SO_REUSEADDR) failed");
                }

                memset(&servaddr, 0, sizeof(servaddr));
                this->addr = addr;

                servaddr.sin_family    = AF_INET; // IPv4 
                servaddr.sin_addr.s_addr = inet_addr(addr.ip.c_str()); 
                servaddr.sin_port = htons(addr.port);

                if(bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0 ){ 
                    throw std::invalid_argument("[i-a] server socket binding failed to address " + addr.str());
                }
            }
            
            void stop(){
                stop_event = true;
                queue_cv.notify_all();
                close(sockfd);

                if (receiver_thread.joinable()) {
                    receiver_thread.join();
                }
                
                for (auto& t : worker_threads) {
                    if (t.joinable()) {
                        t.join();
                    }
                }
            }

            server_udp(){}
            ~server_udp(){}
    };
}