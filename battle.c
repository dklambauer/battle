#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "battle.h"

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p;
    struct client *head = NULL;
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;

    srand(getpid());

    int listenfd = bindandlisten();
    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    //char buf[256];

    while (1) {
        // make a copy of the setbefore we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;  /* and microseconds */

        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
            printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
        }

        if (nready == -1) {
            perror("select");
            continue;
        }

        if (FD_ISSET(listenfd, &rset)){
            printf("a new client is connecting\n");
            len = sizeof(q);
            if ((clientfd = accept(listenfd, 
                    (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
            write(head->fd, "What is your name?\r\n", 21);
        }
        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) {
                        int result = handleclient(p, &head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

/* There is new activity on the client p's connection.
 * We may need to remove p, buffer input from p, simulate a 
 * turn of the battle, or throw out p's input. Detect and react accordingly.
 * Return -1 if p should be removed from the client list, 0 otherwise.
 */
int handleclient(struct client *p, struct client **top) {
    if(p->quit) {
        // p has already quit and just needs to be cleaned up
	    return -1;
    }
    if(p->buffering) {
        // p's input is being saved to its buffer
        int len;
        int num;
        len = buffer(p, top);
        if(len > 0) {
            // We have recieved a full line of input from p
            char buf[128]; // Used to format messsages
            switch(p->buffering) {
            case(1):
                /* buffering name */
                p->buffering = 0;
                strncpy(p->name, p->buffer, 64); 
                printf("Welcome %s!\n", p->name);
                num = sprintf(buf, 
                    "**%s enters the arena**\r\n", p->name);
                broadcast(*top, buf, num + 1);
                num = sprintf(buf,
                    "Welcome, %s! Awaiting opponent...\r\n", p->name);
                write(p->fd, buf, num + 1);
                find_opp(p, *top);
                return 0;
                break;
            case(2):
                /* buffering to speak */
                p->buffering = 0;
                p->buffer[len] = '\r';
                p->buffer[len + 1] = '\n';
                p->buffer[len + 2] = '\0';
                num = sprintf(buf, "You speak: ");
                write(p->fd, buf, num + 1);
                write(p->fd, p->buffer, len + 3);
                num = sprintf(buf, "%s takes a break to tell you: ", p->name);
                write(p->match->opp->fd, buf, num + 1);
                write(p->match->opp->fd, p->buffer, len + 3);
                num = sprintf(buf, "\r\n");
                write(p->fd, buf, num + 1);
                write(p->match->opp->fd, buf, num + 1);
                statprint(p, p->match->opp);
                return 0;
                break;
            default:
                break;
            }
        } else if(len == -1){
            // p is sending too much for the buffer
            write(p->fd, "\r\nSpammers begone!\r\n", 21);
            if(p->match) { 
                forfeit(p->match->opp, p, top); 
            }
            return -1;
        }
        return 0;
    } else if(p->match) {
        if(p->match->turn) {
            /* Currently in a match and p's turn */
            return battlestep(p, p->match->opp, top); /* TODO: value checking */
        } else {
            /* throw out any input entered out of turn */
            if(read(p->fd, p->buffer, 256) <= 0) {
                /* Disconnected user*/
                forfeit(p->match->opp, p, top);
                return -1;
            }
            return 0;
        }
    } 
	/* Throw out anything input out of turn */
	if(read(p->fd, p->buffer, 256) <= 0) {
	    /* Disconnected user*/
	    return -1;
	}
    return 0;
}

/* Attempt to set up a match for p. 
 * Return 0 if we succeeded, 1 if we failed, -1 if p should be removed.
 */
int find_opp(struct client *p, struct client *top) {
    struct client *curr;
    char buf[256];
    int len;
    if(p->quit) { 
	return -1;
    }
    /* Attempt to find p an opponent */
    for(curr = top; curr; curr = curr->next) {
	/* Check if p and curr can be matched */
	if(!curr->quit && curr != p && !curr->match && !curr->buffering && 
	    (p->lastfought != curr || curr->lastfought != p)) {
	    /* Set up new match between p and curr */
	    p->match = malloc(sizeof(struct game));
	    p->match->hp = 20 + (rand() % 11);
	    p->match->pmoves = 1 + (rand() % 3);
	    p->match->opp = curr;
	    p->lastfought = curr;
	    
	    curr->match = malloc(sizeof(struct game));
	    curr->match->hp = 20 + (rand() % 11);
	    curr->match->pmoves = 1 + (rand() % 3);
	    curr->match->opp = p;
	    curr->lastfought = p;
	    
	    if(rand() % 2) {
		p->match->turn = 0;
		curr->match->turn = 1;
	    } else {
		p->match->turn = 1;
		curr->match->turn = 0;
	    }

	    len = sprintf(buf, "You engage %s!\r\n", curr->name);
	    write(p->fd, buf, len);
	    len = sprintf(buf, "You engage %s!\r\n", p->name);
	    write(curr->fd, buf, len);
	    statprint(p, curr);

	    return 0;
	}
    }
    return 1;
}

 /* bind and listen, abort on error
  * returns FD of listening socket
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, 
	    SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->quit = 0;
    p->next = top;
    p->lastfought = NULL;
    p->match = NULL;
    p->name[0] = '\0';
    p->buffer[0] = '\0';
    p->currchar = p->buffer;
    p->bufferroom = 64;
    p->buffering = 1;


    top = p;
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}

/* Move p to the end of the list of clients headed by *top */
int move_to_end(struct client *p, struct client **top) {
    struct client *curr;
    struct client *last = NULL;
    struct client *pred = NULL;
    if(!p->next) {
        return 0;
    }

    for(curr = *top; curr; curr = curr->next) {
        if(curr == p) {
            pred = last;
        }
        last = curr;
    }
    if(pred) {
        // Remove p from its spot in the list of clients
        pred->next = p->next;
    } else {
        if(*top) {
            *top = (*top)->next;
        } else {
            return 0;
        }
    }
    // At this point last is the last client
    last->next = p;
    p->next = NULL;
    return 0;
}

/* Read input from the attacking player p and execute their commands. 
 * Return -1 if the attacker disconnected, 0 otherwise.
 */
int battlestep(struct client *p, struct client *e, struct client **top) {
    int nbytes;
    int len;
    char opt;
    char buf[256];
    /* Read a single char from p and react accordingly*/
    if((nbytes = read(p->fd, &opt, 1)) > 0) {
	write(p->fd, "\r\n", 3);
	switch(opt) {
	    case('a'):
		/* Attack code */
		attack(p, e, top, 0);
		break;
	    case('p'):
		if(p->match->pmoves > 0) {
		    /* Power move code */
		    attack(p, e, top, 1);
		}
		break;
	    case('s'):
		/* Speak code */
		len = sprintf(buf, "Speak: ");
		write(p->fd, buf, len  + 1);

		p->buffering = 2;
		p->currchar = p->buffer;
		p->bufferroom = 256 - 2;
		break;
	    default:
		break;
	}
    } else {
        //disconnected. 
        forfeit(e, p, top);
        return -1;
    }
    return 0;
}

/* Attack e with p, using a powermove if ispowermove is nonzero.
 * Return 0 if this attack did not defeat e.
 */
int attack(struct client *p, struct client *e, 
	    struct client **top, int ispowermove) {
    char buf[256];
    int len;

    if(ispowermove) {
	p->match->pmoves -= 1;
	if(rand() % 2) {
	    int dam = 3 * (2 + rand() % 4);
	    e->match->hp -= dam; 
	
	    len = sprintf(buf, "\r\nYou hit %s for %i damage!\r\n", 
			    e->name, dam);
	    write(p->fd, buf, len + 1);
	    len = sprintf(buf, "\r\n%s powermoves you for %i damage!\r\n", 
			    p->name, dam);
	    write(e->fd, buf, len + 1);

	} else {
	    len = sprintf(buf, "\r\nYou missed!\r\n");
	    write(p->fd, buf, len + 1);
	    len = sprintf(buf, "\r\n%s missed you!\r\n", p->name);
	    write(e->fd, buf, len + 1);
	}
    } else {	
        // Normal attack:
	int dam = 2 + rand() % 4;
	e->match->hp -= dam;
	len = sprintf(buf, "\r\nYou hit %s for %i damage!\r\n", e->name, dam);
	write(p->fd, buf, len + 1);
	len = sprintf(buf, "\r\n%s hits you for %i damage!\r\n", p->name, dam);
	write(e->fd, buf, len + 1);
    }

    // Did this defeat e?
    if(e->match->hp <= 0) {
	len = sprintf(buf, "You are no match for %s. You scurry away...\r\n",
		    p->name);
	write(e->fd, buf, len + 1);
	len = sprintf(buf, "%s gives up. You win!\r\n", e->name);
	write(p->fd, buf, len + 1);
	len = sprintf(buf, "\r\nAwaiting next opponent...\r\n");
	write(p->fd, buf, len + 1);
	write(e->fd, buf, len + 1);

	free(p->match);
	p->match = NULL;
	free(e->match);
	e->match = NULL;
        move_to_end(p, top);
        move_to_end(e, top);
        find_opp(p, *top);
        find_opp(e, *top);
	return 1;
    }
    
    // This advances the turn
    p->match->turn = e->match->turn;
    e->match->turn = (e->match->turn + 1) % 2;
    statprint(p, e);
    return 0;
}

/* Make e forfeit the match and quit. p then looks for a new opponent. */
int forfeit(struct client *p, struct client*e, struct client **top) {
    char buf[256];
    int len;
    
    len = sprintf(buf, "%s dropped. You win!\r\n", e->name);
    write(p->fd, buf, len);
    len = sprintf(buf, "** %s left the arena**\r\n", e->name);
    broadcast(*top, buf, len + 1);

    free(p->match);
    p->match = NULL;
    p->lastfought = NULL;
    free(e->match);
    e->match = NULL;
    e->quit = 1;
    move_to_end(p, top);
    find_opp(p, *top);
    return 0;
}

/* Send a message to all clients */
static void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        write(p->fd, s, size);
    }
}

/* Return the position of the start of the newline (either \n or \r\n)
 * or -1 if it is not in buf 
 */
int find_newline(char *buf, int inbuf) {
  int i;
  for(i = 0; i < inbuf; i++) {
    if((buf[i] == '\r' && buf[i+1] == '\n') || buf[i] == '\n') { return i; }
  }
  return -1; // return the location of the start of the newline if found
}

/* Read input (once) from p and save it to p->buffer. Return the position
 * of the start of a newline in p->buffer if it exists, else 0 or -1, 
 * depending on whether there is room left for a newline in the buffer
 */
int buffer(struct client *p, struct client **top) {
    int nbytes;
    int where;
    if((nbytes = read(p->fd, p->currchar, p->bufferroom)) > 0) {
        p->currchar += nbytes;
        p->bufferroom -= nbytes;
        where = find_newline(p->buffer, p->currchar - p->buffer);
        if(where >= 0) {
            p->buffer[where] = '\0';
            return where;
        }
        if(p->bufferroom == 0) {
            if(p->match) {
                forfeit(p->lastfought, p, top); 
            }
            return -1;
        }
        return 0;
    } else {
	/* Disconnected user */
        if(p->match) {
            forfeit(p->lastfought, p, top); 
        }
        return -1;
    }
}

/* Print match status information to each player */
int statprint(struct client *p, struct client *e) {
    char buf[256];
    int len;

    len = sprintf(buf, "Your hitpoints: %i\r\n" 
	    "Your powermoves: %i\r\n\r\n" 
	    "%s's hitpoints: %i\r\n",
	    p->match->hp, p->match->pmoves, e->name, e->match->hp);
    write(p->fd, buf, len + 1);
    //printf(buf);
    len = sprintf(buf, "Your hitpoints: %i\r\n" 
	    "Your powermoves: %i\r\n\r\n" 
	    "%s's hitpoints: %i\r\n", 
	    e->match->hp, e->match->pmoves, p->name, p->match->hp);
    write(e->fd, buf, len + 1);
    //printf(buf);

    if((p->match->turn && p->match->pmoves) || 
	    (e->match->turn && e->match->pmoves)) {
	len = sprintf(buf, "\r\n(a)ttack\r\n(p)owermove"
			"\r\n(s)peak something\r\n");
    } else {
	len = sprintf(buf, "\r\n(a)ttack\r\n(s)peak something\r\n");
    }
    if(p->match->turn) {
	write(p->fd, buf, len + 1);
	len = sprintf(buf, "Waiting for %s to strike...\n", p->name);
	write(e->fd, buf, len);
    } else {
	write(e->fd, buf, len + 1);
	len = sprintf(buf, "Waiting for %s to strike...\n", e->name);
	write(p->fd, buf, len);
    }
    return 0;
}
