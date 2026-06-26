# Logger
Since Hailo driver may produce large number of logs, there is need for efficient and light weight logging mechanism. The idea is that message is initially produced into simple ring-buffer in thread safe manner for hot path code (driver) and than there is asynchronous down stream thread which consumes messages.

## Ring buffer
Ring buffer is allocated and shared within single logging context. It is at least of page size. When data is to be logged, thread which wishes to log a message will acquire a continous portion of ring buffer. To do so we use CAS on write pointer to move it message length forward. If thread manages to acquire such portion message is written into the buffer. If there is no space in the buffer since asynchronous thread fails to consume messages at fast enough pace message is ignored.

Additionally to simplify hot path code, ring buffer is mirrored. Ring buffer is allocated with whole pages. Additionally one page of virtuall address is mapped to the initial physical page of the buffer. This way we create "alias" to intial page. Accesses will be wrapped around to initial ring buffer address. Since we create mirror mapping of one page, single message cannot be larger in bytes than page size (int this case due to the fact that buffer for a message is stack allocated for speed it is limited even further to 512 bytes).

1. Trivial case
```
|------------------------------ BUFFER ---------------------------------|
|----wr_ptr|        REQUESTED SEGMENT            |-----rd_ptr-----------| ALIASED BACK...
```
2. We can increment wr_ptr of queue normally, but we simply memcpy to wr_ptr + len in the log function. This actually the same as trivial case due to whole buffer being mirrored.
```
|------------------------------ BUFFER ---------------------------------|
| cont... |------------rd_ptr--------------------wr_ptr|  REQUESTED...  | ALIASED BACK...
```
3. If circular distance between rd_ptr and wr_ptr to small ignore message.

## Asynchronous thread
User might register set of callbacks that are later used whenever there is data to log/persist in the ring buffer. User callbacks are periodically called by the logging context, and are provided with buffer to write. This buffer contains data logged into ring buffer.

