#include "prog2.h"
#define MAX_BUFFER_SIZE (100)
#define WINDOW_SIZE (8)


/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
/* Private Variables for A */
struct pkt buffer_queue[MAX_BUFFER_SIZE];
volatile int tip_queue;
volatile int tail_queue;
volatile int size_queue;
volatile int send_next;

int next_seqnum;
int next_acknum;
int A_expected_acknum;


/* Private Variables for B */
int B_expected_seqnum;
int B_last_ack;

void fill_checksum(struct pkt *packet){
  // Our checksum will be 64 bits for wraparound
  uint64_t checksum = 0;

  // First add all the numbes in the packet
  checksum += packet->seqnum + packet->acknum;
  for(int i = 0; i < 20; i++)
    checksum += packet->payload[i];

  // Then check if we need to wraparound
  if(checksum >> 32 != 0)
    checksum++;
  // Now erase the bits above position 32
  checksum = (~checksum) & 0x00000000FFFFFFFF;

  packet->checksum = checksum;
}

int is_corrupt(struct pkt packet){
  uint64_t sum = 0;
  for(int i = 0; i < 20; i++)
    sum += packet.payload[i];

  sum += packet.seqnum;
  sum += packet.acknum;

  // Then check if we need to wraparound
  if(sum >> 32 != 0)
    sum++;
  // Now erase the bits above position 32
  sum = (sum + packet.checksum) & 0x00000000FFFFFFFF;

  return ~sum;
}

void debug_payload(char *data){
  printf("payload: ");
  for(int i = 0; i < 20; i++)
    printf("%c", data[i]);
  printf("\n");
}

void debug_packet(struct pkt packet){
  printf("time        = %f\n", time);
  printf("seqnum      = %d\n", packet.seqnum);
  printf("acknum      = %d\n", packet.acknum);
  printf("checksum    = %u\n", ~packet.checksum);
  printf("size_queue  = %d\n", size_queue);
  printf("tip_queue   = %d\n", tip_queue);
  printf("tail_queue  = %d\n", tail_queue);
  debug_payload(packet.payload);
}

// Here we implement a circular queue
void circular_increment(volatile int *x, int max){
  *x = (*x + 1) % max;
}

void circular_increment1(volatile int *x, int max, int incr){
  *x = (*x + incr) % max;
}

// This function will
int queue_msg(struct msg message){
  if(size_queue == MAX_BUFFER_SIZE){
    fprintf(stderr, "Buffer is full, ABORT\n");
    exit(1);
  }

  struct pkt *packet = &buffer_queue[tail_queue];

  packet->seqnum = next_seqnum;
  circular_increment(&next_seqnum, WINDOW_SIZE * 2);
  packet->acknum = next_acknum;
  circular_increment(&next_acknum, WINDOW_SIZE);

  // Copy the payload
  strncpy(packet->payload, message.data, 20);
  fill_checksum(packet);

  size_queue++;
  circular_increment(&tail_queue, MAX_BUFFER_SIZE - 1);

  debug_packet(*packet);
  return 1;
}

// This function sends all data buffered
// If resending != 0 then it will send the whole window
// Otherwise it will send starting from the first unsent packet
int send_window(int resending){
  if(size_queue == 0 || (!resending && send_next == tail_queue)){
    /* A_expected_acknum = -1; */
    return 0;
  }

  // effective_size = MAX(size_queue, WINDOW_SIZE)
  int effective_size;
  int startfrom, endat;
  endat = tip_queue;

  if(size_queue < WINDOW_SIZE)
    effective_size = size_queue;
  else
    effective_size = WINDOW_SIZE;
  printf("Effective_size: %d\n", effective_size);

  circular_increment1(&endat, MAX_BUFFER_SIZE, effective_size);
  printf("end at: %d\n", endat);

  if(resending)
    startfrom = tip_queue;
  else
    startfrom = send_next;

  printf("send next (before sending): %d\n", send_next);
  // Send the whole window
  for(int i = startfrom; i != endat; circular_increment(&i, MAX_BUFFER_SIZE)){
    printf("Sending i = %d\n", i);
    tolayer3(0, buffer_queue[i]);

    // dummy statement to keep compiler happy
    i = (int) i;
    if(!resending)
      circular_increment(&send_next, MAX_BUFFER_SIZE);
  }
  /* circular_increment1(&send_next, MAX_BUFFER_SIZE, effective_size + 1); */
  /* printf("Effective_size: %d\n", effective_size); */
  /* printf("send next (before sending): %d\n", send_next); */
  /* send_next = (send_next + effective_size) % MAX_BUFFER_SIZE; */
  printf("send next (after sending): %d\n", send_next);

  return effective_size;
}

int dequeue(){
  if(size_queue == 0){
    fprintf(stderr, "Trying to dequeue empty buffer!!");
    return 0;
  }

  size_queue--;
  circular_increment(&tip_queue, MAX_BUFFER_SIZE - 1);
  return 1;
}

int dequeue_until(int acknum){
  if(size_queue == 0){
    fprintf(stderr, "Trying to dequeue empty buffer!!");
    return 0;
  }

  while(buffer_queue[tip_queue].acknum != acknum){
    size_queue--;
    circular_increment(&tip_queue, MAX_BUFFER_SIZE - 1);
  }
  size_queue--;
  circular_increment(&tip_queue, MAX_BUFFER_SIZE - 1);

  return 1;
}

/* called from layer 5, passed the data to be sent to other side */
int A_output(struct msg message)
{
  printf("\n-------------- A output --------------\n");
  printf("Sending Packet at A: \n");
  /* debug_payload(message.data); */

  queue_msg(message);

  if(tip_queue == send_next)
    starttimer(0, 20.0);

  send_window(0);

  return 0;
}

/* called from layer 3, when a packet arrives for layer 4 */
int A_input(struct pkt packet)
{
  printf("\n-------------- A input --------------\n");
  debug_packet(packet);

  stoptimer(0);

  if(!is_corrupt(packet)){
    /* circular_increment1(&tip_queue, MAX_BUFFER_SIZE, packet.acknum); */
    dequeue_until(packet.acknum);
    send_window(0);
  }
  else{
    printf("Packet was corrupted, resending the whole window\n");
    send_window(1);
  }

  if(tip_queue != send_next)
    starttimer(0, 20.0);

  return 0;
}

/* called when A's timer goes off */
int A_timerinterrupt() {
  printf("\n-------------- A timeout --------------\n");
  printf("The packet was lost, resending the whole window\n");

  send_window(1);

  starttimer(0, 20.0);

  return 0;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
int A_init() {
  send_next = 0;
  tip_queue = 0;
  tail_queue = 0;
  size_queue = 0;
  next_seqnum = 0;
  next_acknum = 0;
  /* A_expected_acknum = -1; */

  return 0;
}

int fill_ack(struct pkt *packet, int ack){
  memset(&packet->payload, 0, sizeof(struct pkt));
  strcpy(packet->payload, "xxXXXxxxXXXxxxXXXxx");
  packet->acknum = ack;
  fill_checksum(packet);

  return 1;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */
/* called from layer 3, when a packet arrives for layer 4 at B*/
int B_input(struct pkt packet)
{
  printf("\n-------------- B output --------------\n");
  printf("Got a packet with\n");
  debug_packet(packet);

  if(packet.seqnum != B_expected_seqnum){
    printf("Packet out of order, dropping packet\n");
    /* packet.acknum = B_last_ack; */
    return 0;
  }
  else if(is_corrupt(packet)){
    printf("Packet was corrupted, sending NACK\n");
    /* packet.acknum = B_last_ack; */
    /* fill_ack(&packet, B_last_ack); */
  }
  else {
    circular_increment(&B_expected_seqnum, WINDOW_SIZE * 2);
    circular_increment(&B_last_ack, WINDOW_SIZE);
    tolayer5(1, packet.payload);
  }

  // We will simply resend the same packet, its no problem
  // if it is corrupted because we wish that the sender resends
  // it anyways.
  tolayer3(1, packet);

  return 0;
}

/* called when B's timer goes off */
int B_timerinterrupt() {return 0;}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
int B_init() {
  B_expected_seqnum = 0;
  B_last_ack = 0;

  return 0;
}

int TRACE = 1;   /* for my debugging */
int nsim = 0;    /* number of messages from 5 to 4 so far */
int nsimmax = 0; /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;    /* probability that a packet is dropped  */
float corruptprob; /* probability that one bit is packet is flipped */
float lambda;      /* arrival rate of messages from layer 5 */
int ntolayer3;     /* number sent into layer 3 */
int nlost;         /* number lost in media */
int ncorrupt;      /* number corrupted by media*/

int main()
{
  struct event * eventptr;
  struct msg msg2give;
  struct pkt pkt2give;

  int i, j;

  init();
  A_init();
  B_init();

  for (;; ) {
    eventptr = evlist; /* get next event to simulate */
    if (NULL == eventptr) {
      goto terminate;
    }
    evlist = evlist->next; /* remove this event from event list */
    if (evlist != NULL) {
      evlist->prev = NULL;
    }
    if (TRACE >= 2) {
      printf("\nEVENT time: %f,", eventptr->evtime);
      printf("  type: %d", eventptr->evtype);
      if (eventptr->evtype == 0) {
        printf(", timerinterrupt  ");
      } else if (eventptr->evtype == 1) {
        printf(", fromlayer5 ");
      } else {
        printf(", fromlayer3 ");
      }
      printf(" entity: %d\n", eventptr->eventity);
    }
    time = eventptr->evtime; /* update time to next event time */
    if (eventptr->evtype == FROM_LAYER5) {
      if (nsim == nsimmax) {
        break; /* all done with simulation */
      }
      generate_next_arrival(); /* set up future arrival */
      /* fill in msg to give with string of same letter */
      j = nsim % 26;
      for (i = 0; i < 20; i++) {
        msg2give.data[i] = 97 + j;
      }
      if (TRACE > 2) {
        printf("          MAINLOOP: data given to student: ");
        for (i = 0; i < 20; i++) {
          printf("%c", msg2give.data[i]);
        }
        printf("\n");
      }
      nsim++;
      if (eventptr->eventity == A) {
        A_output(msg2give);
      }
    } else if (eventptr->evtype == FROM_LAYER3) {
      pkt2give.seqnum = eventptr->pktptr->seqnum;
      pkt2give.acknum = eventptr->pktptr->acknum;
      pkt2give.checksum = eventptr->pktptr->checksum;
      for (i = 0; i < 20; i++) {
        pkt2give.payload[i] = eventptr->pktptr->payload[i];
      }
      if (eventptr->eventity == A) { /* deliver packet by calling */
        A_input(pkt2give);           /* appropriate entity */
      } else {
        B_input(pkt2give);
      }
      free(eventptr->pktptr); /* free the memory for packet */
    } else if (eventptr->evtype == TIMER_INTERRUPT) {
      if (eventptr->eventity == A) {
        A_timerinterrupt();
      } else {
        B_timerinterrupt();
      }
    } else {
      printf("INTERNAL PANIC: unknown event type \n");
    }
    free(eventptr);
  }
  return 0;

terminate:
  printevlist();
  printf(
    " Simulator terminated at time %f\n after sending %d msgs from layer5\n",
    time, nsim);
  return 0;
}

void init() /* initialize the simulator */
{
  int i;
  float sum, avg;

  printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
  printf("Enter the number of messages to simulate: ");
  scanf("%d", &nsimmax);
  printf("Enter  packet loss probability [enter 0.0 for no loss]:");
  scanf("%f", &lossprob);
  printf("Enter packet corruption probability [0.0 for no corruption]:");
  scanf("%f", &corruptprob);
  printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
  scanf("%f", &lambda);
  printf("Enter TRACE:");
  scanf("%d", &TRACE);

  srand(rand_seed); /* init random number generator */
  sum = 0.0;   /* test random number generator for students */
  for (i = 0; i < 1000; i++) {
    sum = sum + jimsrand(); /* jimsrand() should be uniform in [0,1] */
  }
  avg = sum / 1000.0;
  if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n");
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(0);
  }

  ntolayer3 = 0;
  nlost = 0;
  ncorrupt = 0;

  time = 0.0;              /* initialize time to 0.0 */
  generate_next_arrival(); /* initialize event list */
}

/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
  double mmm = INT_MAX;         /* largest int  - MACHINE DEPENDENT!!!!!!!!   */
  float x;                      /* individual students may need to change mmm */
  x = rand_r(&rand_seed) / mmm; /* x should be uniform in [0,1] */
  return x;
}

/************ EVENT HANDLINE ROUTINES ****************/
/*  The next set of routines handle the event list   */
/*****************************************************/
void generate_next_arrival()
{
  double x;
  struct event * evptr;

  if (TRACE > 2) {
    printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
  }

  x = lambda * jimsrand() * 2; /* x is uniform on [0,2*lambda] */
  /* having mean of lambda        */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime = time + x;
  evptr->evtype = FROM_LAYER5;
  if (BIDIRECTIONAL && (jimsrand() > 0.5)) {
    evptr->eventity = B;
  } else {
    evptr->eventity = A;
  }
  insertevent(evptr);
}

void insertevent(struct event * p)
{
  struct event * q, * qold;

  if (TRACE > 2) {
    printf("            INSERTEVENT: time is %lf\n", time);
    printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
  }
  q = evlist;      /* q points to header of list in which p struct inserted */
  if (NULL == q) { /* list is empty */
    evlist = p;
    p->next = NULL;
    p->prev = NULL;
  } else {
    for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next) {
      qold = q;
    }
    if (NULL == q) { /* end of list */
      qold->next = p;
      p->prev = qold;
      p->next = NULL;
    } else if (q == evlist) { /* front of list */
      p->next = evlist;
      p->prev = NULL;
      p->next->prev = p;
      evlist = p;
    } else { /* middle of list */
      p->next = q;
      p->prev = q->prev;
      q->prev->next = p;
      q->prev = p;
    }
  }
}

void printevlist()
{
  struct event * q;
  printf("--------------\nEvent List Follows:\n");
  for (q = evlist; q != NULL; q = q->next) {
    printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype,
      q->eventity);
  }
  printf("--------------\n");
}

/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB)
{
  struct event * q;

  if (TRACE > 2) {
    printf("          STOP TIMER: stopping timer at %f\n", time);
  }

  for (q = evlist; q != NULL; q = q->next) {
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
      /* remove this event */
      if (NULL == q->next && NULL == q->prev) {
        evlist = NULL;              /* remove first and only event on list */
      } else if (NULL == q->next) { /* end of list - there is one in front */
        q->prev->next = NULL;
      } else if (q == evlist) { /* front of list - there must be event after */
        q->next->prev = NULL;
        evlist = q->next;
      } else { /* middle of list */
        q->next->prev = q->prev;
        q->prev->next = q->next;
      }
      free(q);
      return;
    }
  }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB, float increment)
{
  struct event * q;
  struct event * evptr;

  if (TRACE > 2) {
    printf("          START TIMER: starting timer at %f\n", time);
  }

  /* be nice: check to see if timer is already started, if so, then  warn */
  for (q = evlist; q != NULL; q = q->next) {
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
    }
  }

  /* create future event for when timer goes off */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtime = time + increment;
  evptr->evtype = TIMER_INTERRUPT;
  evptr->eventity = AorB;
  insertevent(evptr);
}

/************************** TOLAYER3 ***************/
void tolayer3(int AorB, struct pkt packet)
{
  struct pkt * mypktptr;
  struct event * evptr, * q;
  float lastime, x;
  int i;

  ntolayer3++;

  /* simulate losses: */
  if (jimsrand() < lossprob) {
    nlost++;
    if (TRACE > 0) {
      printf("          TOLAYER3: packet being lost\n");
    }
    return;
  }

  /*
   * make a copy of the packet student just gave me since he/she may decide
   * to do something with the packet after we return back to him/her
   */

  mypktptr = (struct pkt *)malloc(sizeof(struct pkt));
  mypktptr->seqnum = packet.seqnum;
  mypktptr->acknum = packet.acknum;
  mypktptr->checksum = packet.checksum;
  for (i = 0; i < 20; ++i) {
    mypktptr->payload[i] = packet.payload[i];
  }
  if (TRACE > 2) {
    printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
      mypktptr->acknum, mypktptr->checksum);
    for (i = 0; i < 20; ++i) {
      printf("%c", mypktptr->payload[i]);
    }
    printf("\n");
  }

  /* create future event for arrival of packet at the other side */
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype = FROM_LAYER3;      /* packet will pop out from layer3 */
  evptr->eventity = (AorB + 1) & 1; /* event occurs at other entity */
  evptr->pktptr = mypktptr;         /* save ptr to my copy of packet */

  /*
   * finally, compute the arrival time of packet at the other end.
   * medium can not reorder, so make sure packet arrives between 1 and 10
   * time units after the latest arrival time of packets
   * currently in the medium on their way to the destination
   */

  lastime = time;
  for (q = evlist; q != NULL; q = q->next) {
    if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity)) {
      lastime = q->evtime;
    }
  }
  evptr->evtime = lastime + 1 + 9 * jimsrand();

  /* simulate corruption: */
  if (jimsrand() < corruptprob) {
    ncorrupt++;
    if ((x = jimsrand()) < .75) {
      mypktptr->payload[0] = 'Z'; /* corrupt payload */
    } else if (x < .875) {
      mypktptr->seqnum = 999999;
    } else {
      mypktptr->acknum = 999999;
    }
    if (TRACE > 0) {
      printf("          TOLAYER3: packet being corrupted\n");
    }
  }

  if (TRACE > 2) {
    printf("          TOLAYER3: scheduling arrival on other side\n");
  }
  insertevent(evptr);
}

void tolayer5(int AorB, const char * datasent)
{
  (void)AorB;
  int i;
  if (TRACE > 2) {
    printf("          TOLAYER5: data received: ");
    for (i = 0; i < 20; i++) {
      printf("%c", datasent[i]);
    }
    printf("\n");
  }
}
