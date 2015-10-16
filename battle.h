#ifndef BATTLE_H
    #define BATTLE_H

#ifndef PORT
    #define PORT 30103
#endif

struct client {
    int fd;
    struct in_addr ipaddr;
    int quit; //1 if should be removed, 0 otherwise
    struct client *next; //The next client in the linked list
    struct client *lastfought; //The client last fought
    struct game *match; //Holds data about the current match
    char name[64]; //The client's name
    char buffer[256]; //Buffer used to hold input/output for the client
    char *currchar; //Current position when buffering input
    int bufferroom; //Remaining space when buffering input
    int buffering; /* 0 if not buffering, 
		    * 1 if buffering the name, 
		    * 2 if buffering to speak
		    */
};

struct game {
   int hp; //The hitpoints of the client that has this game as its .match
   int pmoves; //The powermoves of the client
   int turn; //1 if the client's turn, 0 otherwise
   struct client *opp; //The other client involved in this battle
};

static struct client *addclient(struct client *top, 
				int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);

/* Send a message to all clients */
static void broadcast(struct client *top, char *s, int size);

/* There is new activity on the client p's connection.
 * We may need to remove p, buffer input from p, simulate a 
 * turn of the battle, or throw out p's input. Detect and react accordingly.
 * Return -1 if p should be removed from the client list, 0 otherwise.
 */
int handleclient(struct client *p, struct client **top);

int bindandlisten(void);

/* Move p to the end of the list of clients headed by *top */
int move_to_end(struct client *p, struct client **top);

/* Return the position of the start of the newline (either \n or \r\n)
 * or -1 if it is not in buf 
 */
int find_newline(char *buf, int inbuf);

/* Read input from the attacking player p and execute their commands. 
 * Return -1 if the attacker disconnected, 0 otherwise.
 */
int battlestep(struct client *p, struct client *e, struct client **top);

/* Attempt to set up a match for p. 
 * Return 0 if we succeeded, 1 if we failed, -1 if p should be removed.
 */
int find_opp(struct client *p, struct client *top);

/* Read input (once) from p and save it to p->buffer. Return the position
 * of the start of a newline in p->buffer if it exists, else 0 or -1, 
 * depending on whether there is room left for a newline in the buffer
 */
int buffer(struct client *p, struct client **top);

/* Print match status information to each player */
int statprint(struct client *p, struct client *e);

/* Attack e with p, using a powermove if ispowermove is nonzero.
 * Return 0 if this attack did not defeat e.
 */
int attack(struct client *p, struct client *e, 
	    struct client **top, int ispowermove);

/* Make e forfeit the match and quit. p then looks for a new opponent. */
int forfeit(struct client *p, struct client *e, struct client **top);
#endif
