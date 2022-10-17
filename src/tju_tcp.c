#include "tju_tcp.h"
#include "global.h"
#include "trans.h"
#include "list.h"

static tju_tcp_t *connected_queue[512];
static int connected_queue_pointer = 0;

pthread_mutex_t print_lock = PTHREAD_MUTEX_INITIALIZER;
bitmap *ackmap;

/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/

tju_tcp_t *tju_socket() {
  init_retransmit_timer();
  init_logger();
  ackmap = bitmap_allocate(1000000);

  tju_tcp_t *sock = (tju_tcp_t *) malloc(sizeof(tju_tcp_t));
  sock->state = CLOSED;

  pthread_mutex_init(&(sock->send_lock), NULL);
  sock->sending_queue = newQueue(100000);
  sock->sending_buf = NULL;
  sock->sending_len = 0;

  pthread_mutex_init(&(sock->recv_lock), NULL);
  sock->received_buf = NULL;
  sock->received_len = 0;

  if (pthread_cond_init(&sock->wait_cond, NULL) != 0) {
    perror("ERROR condition variable not set\n");
    exit(-1);
  }

  sock->window.wnd_send = malloc(sizeof(sender_window_t));
  sock->window.wnd_recv = malloc(sizeof(receiver_window_t));

  sock->window.wnd_send->rto = INIT_RTT;
  sock->window.wnd_send->estmated_rtt = INIT_RTT;
  sock->window.wnd_send->dev_rtt = INIT_RTT / 2;

  sock->window.wnd_send->window_size = INIT_SEND_WINDOW;

/*
  sock->window.wnd_recv->received_map =
      hashmap_new(sizeof(tju_packet_t), 100000, 0, 0, packet_hash, packet_compare, NULL, NULL);
*/
  sock->window.wnd_recv->buffer_list = list_init();

  return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t *sock, tju_sock_addr bind_addr) {
  sock->bind_addr = bind_addr;
  return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t *sock) {
  sock->state = LISTEN;
  //DEBUG_PRINT("listening local addr:%d, local port:%d\n", sock->bind_addr.ip,sock->bind_addr.port);
  int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
  listen_socks[hashval] = sock;
  return 0;
}

/*
接受连接
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
tju_tcp_t *tju_accept(tju_tcp_t *listen_sock) {
  while (connected_queue_pointer == 0) {}
  tju_tcp_t *new_conn = connected_queue[0];
  connected_queue_pointer--;
  tju_sock_addr local_addr = new_conn->established_local_addr;
  tju_sock_addr remote_addr = new_conn->established_remote_addr;
  pthread_mutex_init(&(new_conn->recv_lock), NULL);
  // 将新的conn放到内核建立连接的socket哈希表中
  int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
  DEBUG_PRINT("new TCP connection established, with hashval:%d \n", hashval);
  established_socks[hashval] = new_conn;

  // 如果new_conn的创建过程放到了tju_handle_packet中 那么accept怎么拿到这个new_conn呢
  // 在linux中 每个listen socket都维护一个已经完成连接的socket队列
  // 每次调用accept 实际上就是取出这个队列中的一个元素
  // 队列为空,则阻塞
  return new_conn;
}

/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tju_connect(tju_tcp_t *sock, tju_sock_addr target_addr) {

  sock->established_remote_addr = target_addr;

  tju_sock_addr local_addr;
  local_addr.ip = inet_network("172.17.0.2");
  local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
  sock->established_local_addr = local_addr;

  // 这里也不能直接建立连接 需要经过三次握手
  // 实际在linux中 connect调用后 会进入一个while循环
  // 循环跳出的条件是socket的状态变为ESTABLISHED 表面看上去就是 正在连接中 阻塞
  // 而状态的改变在别的地方进行 在我们这就是tju_handle_packet
  //SEND SYN
  sock->window.wnd_send->nextseq = INIT_CLIENT_SEQ;
  sock->window.wnd_send->base = INIT_CLIENT_SEQ;

  tju_packet_t
      *pkt = create_packet(sock->established_local_addr.port,
                           target_addr.port,
                           sock->window.wnd_send->nextseq,
                           0,
                           DEFAULT_HEADER_LEN,
                           DEFAULT_HEADER_LEN,
                           SYN_FLAG_MASK,
                           get_list_remain_size(sock->window.wnd_recv->buffer_list),
                           0,
                           NULL,
                           0);

  auto_retransmit(sock, pkt, TRUE);

  sock->window.wnd_send->nextseq++;

  sock->state = SYN_SENT;
  DEBUG_PRINT("SYN SENT\n");
  int hashval = cal_hash(local_addr.ip, local_addr.port, 0, 0);
  listen_socks[hashval] = sock;
  //DEBUG_PRINT("listen on hashval:%d\n", hashval);

  while (sock->state != ESTABLISHED) {
  }
  sock->window.wnd_send->cwnd_state = SLOW_START;
  sock->window.wnd_send->cwnd = 1;
  sock->window.wnd_send->ssthresh = INIT_SEND_WINDOW;
  listen_socks[hashval] = NULL;

  // 将建立了连接的socket放入内核 已建立连接哈希表中
  hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
  established_socks[hashval] = sock;

  DEBUG_PRINT("Target Server Connected!\n");

  return 0;
}

int send_packet(tju_packet_t *packet_to_send) {
  char *msg = packet_to_buf(packet_to_send);
  sendToLayer3(msg, packet_to_send->header.plen);
  log_send_event(packet_to_send->header.seq_num, packet_to_send->header.ack_num, packet_to_send->header.flags);
  free(msg);
  return 0;
}

int tju_send(tju_tcp_t *sock, const void *buffer, int len) {
  //DEBUG_PRINT("message to send:%s\n", (char *) buffer);
  pthread_mutex_lock(&sock->send_lock);
  int count = len / MAX_DLEN;
  for (int i = 0; i <= count; ++i) {
    int send_len = (i == count) ? len % MAX_DLEN : MAX_DLEN;
    tju_packet_t *pkt = create_packet(sock->established_local_addr.port,
                                      sock->established_remote_addr.port,
                                      sock->window.wnd_send->nextseq,
                                      0,
                                      DEFAULT_HEADER_LEN,
                                      DEFAULT_HEADER_LEN + send_len,
                                      NO_FLAG,
                                      get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                      0,
                                      (char *) buffer + i * MAX_DLEN,
                                      send_len);
    enqueue(sock->sending_queue, pkt);
    sock->window.wnd_send->nextseq += pkt->header.plen - DEFAULT_HEADER_LEN + 1;
  }

  while (size(sock->sending_queue) > 0) {
    tju_packet_t *pkt = dequeue(sock->sending_queue);
    while (sock->window.wnd_send->base + sock->window.wnd_send->window_size * MAX_DLEN
        < sock->window.wnd_send->sent_seq) {
    }
    auto_retransmit(sock, pkt, TRUE);
    sock->window.wnd_send->sent_seq = pkt->header.seq_num;
  }
  pthread_mutex_unlock(&sock->send_lock);
  return 1;
}
int tju_recv(tju_tcp_t *sock, void *buffer, int len) {
  while (sock->received_len <= 0) {
    // 阻塞
  }

  pthread_mutex_lock(&(sock->recv_lock)); // 加锁

  uint32_t read_len = 0;
  if (sock->received_len >= len) { // 从中读取len长度的数据
    read_len = len;
  } else {
    read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
  }

  memcpy(buffer, sock->received_buf, read_len);

  if (read_len < sock->received_len) { // 还剩下一些
    char *new_buf = malloc(sock->received_len - read_len);
    memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
    free(sock->received_buf);
    sock->received_len -= read_len;
    sock->received_buf = new_buf;
  } else {
    free(sock->received_buf);
    sock->received_buf = NULL;
    sock->received_len = 0;
  }
  pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

  return read_len;
}

int tju_handle_packet(tju_tcp_t *sock, char *pkt) {

  uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

  // 把收到的数据放到接受缓冲区
  pthread_mutex_lock(&(sock->recv_lock)); // 加锁

  uint8_t flag = get_flags(pkt);
  uint32_t seq = get_seq(pkt);
  uint32_t ack = get_ack(pkt);
  uint16_t rwnd = get_advertised_window(pkt);
  if (flag & ACK_FLAG_MASK) {
    on_ack_received(ack, sock, rwnd);
  }
  uint16_t src_port = get_src(pkt);
  uint16_t dst_port = get_dst(pkt);

  log_recv_event(seq, ack, flag);

  tju_tcp_t *new_conn = NULL;

  //DEBUG_PRINT("STATE:%d\n", sock->state);

  switch (sock->state) {
    case LISTEN:
      if (flag == SYN_FLAG_MASK) {
        DEBUG_PRINT("SYN_FLAG RECEIVED\n");
        sock->state = SYN_RECV;
        sock->window.wnd_send->nextseq = INIT_SERVER_SEQ;
        sock->window.wnd_send->base = INIT_SERVER_SEQ;
        //SEND SYN ACK
        tju_packet_t *pkt = create_packet(dst_port,
                                          src_port,
                                          sock->window.wnd_send->nextseq,
                                          seq + 1,
                                          DEFAULT_HEADER_LEN,
                                          DEFAULT_HEADER_LEN,
                                          SYN_FLAG_MASK | ACK_FLAG_MASK,
                                          get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                          0,
                                          NULL,
                                          0);
        auto_retransmit(sock, pkt, TRUE);
        sock->window.wnd_send->nextseq += 1;
        DEBUG_PRINT("SYN_ACK SENT\n");
        sock->window.wnd_recv->expect_seq = seq + 1;
        DEBUG_PRINT("expect_seq:%d\n", sock->window.wnd_recv->expect_seq);
      }
      break;
    case SYN_SENT:
      if (flag == (SYN_FLAG_MASK | ACK_FLAG_MASK)) {
        DEBUG_PRINT("SYN_ACK RECEIVED\n");
        sock->state = ESTABLISHED;

        //SEND ACK
        tju_packet_t *pkt = create_packet(dst_port,
                                          src_port,
                                          sock->window.wnd_send->nextseq,
                                          seq + 1,
                                          DEFAULT_HEADER_LEN,
                                          DEFAULT_HEADER_LEN,
                                          ACK_FLAG_MASK,
                                          get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                          0,
                                          NULL,
                                          0);
        auto_retransmit(sock, pkt, FALSE);
        DEBUG_PRINT("ACK SENT\n");
        sock->window.wnd_recv->expect_seq = seq + 1;
        sock->window.wnd_send->sent_seq = sock->window.wnd_send->base;
        DEBUG_PRINT("expect_seq:%d\n", sock->window.wnd_recv->expect_seq);
      }
      break;
    case SYN_RECV:
      if (flag == ACK_FLAG_MASK) {
        DEBUG_PRINT("ACK RECEIVED\n");
        new_conn = (tju_tcp_t *) malloc(sizeof(tju_tcp_t));
        memcpy(new_conn, sock, sizeof(tju_tcp_t));
        new_conn->established_local_addr = sock->bind_addr;
        new_conn->established_remote_addr.port = src_port;
        new_conn->established_remote_addr.ip = inet_network("172.17.0.2");
        new_conn->state = ESTABLISHED;
        new_conn->window.wnd_send->sent_seq = new_conn->window.wnd_send->base;
        connected_queue[connected_queue_pointer++] = new_conn;
        sock->state = ESTABLISHED;
      }
      break;
    case ESTABLISHED:
      if (flag == NO_FLAG) {
        DEBUG_PRINT("PKT received with seq: %d, dlen: %d\n", seq, data_len);
        tju_packet_t *ack_pkt = create_packet(dst_port,
                                              src_port,
                                              0,
                                              seq + data_len + 1,
                                              DEFAULT_HEADER_LEN,
                                              DEFAULT_HEADER_LEN,
                                              ACK_FLAG_MASK,
                                              get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                              0,
                                              NULL,
                                              0);
        auto_retransmit(sock, ack_pkt, FALSE);
        free_packet(ack_pkt);
        if (data_len > 0) {
          //hashmap_set(sock->window.wnd_recv->received_map, buf_to_packet(pkt));
          list_push(sock->window.wnd_recv->buffer_list, seq, buf_to_packet(pkt));
          if (sock->window.wnd_recv->expect_seq == seq) {
            DEBUG_PRINT("expect_seq:%d updated!\n", sock->window.wnd_recv->expect_seq);
            if (sock->received_buf == NULL || sock->received_len == 0) {
              sock->received_buf = malloc(data_len);
            } else {
              sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
            }
            memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
            log_delv_event(seq, data_len);
            sock->received_len += data_len;
            sock->window.wnd_recv->expect_seq += data_len + 1;
            DEBUG_PRINT("expect_seq:%d\n", sock->window.wnd_recv->expect_seq);
            tju_packet_t *tmp = NULL;
/*
            tmp = hashmap_get(sock->window.wnd_recv->received_map,
                              &(tju_packet_t) {.header.seq_num = sock->window.wnd_recv->expect_seq});
*/
            tmp = list_pop(sock->window.wnd_recv->buffer_list, sock->window.wnd_recv->expect_seq);
            while (tmp) {
              DEBUG_PRINT("Find buffered packet with seq: %d\n", tmp->header.seq_num);
              uint32_t next_data_len = tmp->header.plen - DEFAULT_HEADER_LEN;
              sock->received_buf = realloc(sock->received_buf, sock->received_len + next_data_len);
              memcpy(sock->received_buf + sock->received_len, tmp->data, next_data_len);
              log_delv_event(tmp->header.seq_num, next_data_len);
              sock->received_len += next_data_len;
              free(tmp->data);
              //hashmap_delete(sock->window.wnd_recv->received_map, tmp);
              free(tmp);
              sock->window.wnd_recv->expect_seq += next_data_len + 1;
              DEBUG_PRINT("expect_seq:%d\n", sock->window.wnd_recv->expect_seq);
              tmp = list_pop(sock->window.wnd_recv->buffer_list, sock->window.wnd_recv->expect_seq);
/*
              tmp = hashmap_get(sock->window.wnd_recv->received_map,
                                &(tju_packet_t) {.header.seq_num = sock->window.wnd_recv->expect_seq});
*/
            }
          }
        }
      } else if (flag == ACK_FLAG_MASK) {
        DEBUG_PRINT("new ACK RECEIVED\n");
      } else if (flag == FIN_FLAG_MASK | ACK_FLAG_MASK) {
        sock->state = CLOSE_WAIT;

        //SEND ACK
        tju_packet_t *pkt = create_packet(dst_port,
                                          src_port,
                                          ack,
                                          seq + 1,
                                          DEFAULT_HEADER_LEN,
                                          DEFAULT_HEADER_LEN,
                                          ACK_FLAG_MASK,
                                          get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                          0,
                                          NULL,
                                          0);
        auto_retransmit(sock, pkt, FALSE);
        DEBUG_PRINT("ACK SENT\n");
        sock->window.wnd_recv->expect_seq = seq + 1;
        DEBUG_PRINT("seq:%d,ack:%d\n", ack, seq + 1);
        sleep(1);    //necessary?
        tju_packet_t *ppkt = create_packet(dst_port,
                                           src_port,
                                           ack,
                                           seq + 1,
                                           DEFAULT_HEADER_LEN,
                                           DEFAULT_HEADER_LEN,
                                           ACK_FLAG_MASK | FIN_FLAG_MASK,
                                           get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                           0,
                                           NULL,
                                           0);
        auto_retransmit(sock, ppkt, FALSE);
        DEBUG_PRINT("FINACK SENT\n");
        sock->window.wnd_recv->expect_seq = seq + 1;
        DEBUG_PRINT("seq:%d,ack:%d\n", ack, seq + 1);
        sock->state = LAST_ACK;
      }
      break;
    case FIN_WAIT_1:
      if (flag == ACK_FLAG_MASK) {
        //sock->window.wnd_recv->expect_seq = seq + 1;
        sock->state = FIN_WAIT_2;
      } else if (flag == (ACK_FLAG_MASK | FIN_FLAG_MASK)) {
        //fflag++;
        sock->state = CLOSING;
        DEBUG_PRINT("CLOSING!\n");
        sleep(3);

        //SEND ACK
        // while(fflag!=2){

        // }
        tju_packet_t *pkt = create_packet(dst_port,
                                          src_port,
                                          sock->window.wnd_send->nextseq,
                                          seq + 1,
                                          DEFAULT_HEADER_LEN,
                                          DEFAULT_HEADER_LEN,
                                          ACK_FLAG_MASK,
                                          get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                          0,
                                          NULL,
                                          0);
        auto_retransmit(sock, pkt, FALSE);
        DEBUG_PRINT("ACK SENT\n");
        DEBUG_PRINT("seq:%d,ack:%d\n", sock->window.wnd_send->nextseq, seq + 1);
        //sock->window.wnd_send->nextseq++;
        sock->window.wnd_recv->expect_seq = seq + 1;
      }

      break;
    case FIN_WAIT_2:
      if (flag == (FIN_FLAG_MASK | ACK_FLAG_MASK)) {
        sock->state = TIME_WAIT;

        //SEND ACK
        tju_packet_t *pkt = create_packet(dst_port,
                                          src_port,
                                          ack,
                                          seq + 1,
                                          DEFAULT_HEADER_LEN,
                                          DEFAULT_HEADER_LEN,
                                          ACK_FLAG_MASK,
                                          get_list_remain_size(sock->window.wnd_recv->buffer_list),
                                          0,
                                          NULL,
                                          0);
        auto_retransmit(sock, pkt, FALSE);
        DEBUG_PRINT("ACK SENT\n");
        DEBUG_PRINT("seq:%d,ack:%d\n", ack, seq + 1);
        //sock->window.wnd_send->nextseq++;
        sock->window.wnd_recv->expect_seq = seq + 1;
        sleep(2);
        sock->state = CLOSED;
      }

      break;
    case CLOSING:
      if (flag == ACK_FLAG_MASK) {
        sock->state = TIME_WAIT;
        sleep(2);
        sock->state = CLOSED;

        //sock->window.wnd_recv->expect_seq = seq + 1;
      }
      break;
    case LAST_ACK:
      if (flag == ACK_FLAG_MASK) {
        sock->state = CLOSED;

        //sock->window.wnd_recv->expect_seq = seq + 1;
      }
      break;
  }

/*
  if (data_len > 0) {
    if (sock->received_buf == NULL) {
      sock->received_buf = malloc(data_len);
    } else {
      if (sock->received_len == 0) {
        sock->received_buf = malloc(data_len);
      } else {
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
      }
    }
    memset(sock->received_buf + sock->received_len, 0, data_len);
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    sock->received_len += data_len;
  }
*/
  pthread_mutex_unlock(&(sock->recv_lock)); // 解锁


  return 0;
}

int tju_close(tju_tcp_t *sock) {
  while (size(sock->sending_queue) > 0) {

  }
  sock->state = FIN_WAIT_1;
  DEBUG_PRINT("FIN_WAIT_1!\n");
  sleep(1);

  tju_sock_addr local_addr;
  local_addr = sock->established_local_addr;
  tju_sock_addr target_addr;
  target_addr = sock->established_remote_addr;

  tju_packet_t
      *pkt = create_packet(sock->established_local_addr.port,
                           target_addr.port,
                           sock->window.wnd_send->nextseq,
                           sock->window.wnd_recv->expect_seq,
                           DEFAULT_HEADER_LEN,
                           DEFAULT_HEADER_LEN,
                           FIN_FLAG_MASK | ACK_FLAG_MASK,
                           get_list_remain_size(sock->window.wnd_recv->buffer_list),
                           0,
                           NULL,
                           0);

  auto_retransmit(sock, pkt, FALSE);
  DEBUG_PRINT("sent FINACK!\n");
  DEBUG_PRINT("seq:%d,ack:%d\n", sock->window.wnd_send->nextseq, sock->window.wnd_recv->expect_seq);
  sock->window.wnd_send->nextseq++;

  while (sock->state != CLOSED) {

  }
  int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);

  established_socks[hashval] = NULL;
  freeQueue(sock->sending_queue);
  sock = NULL;
  DEBUG_PRINT("Closed!\n");

  return 0;
}
