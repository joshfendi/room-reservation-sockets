Joshua Fendi

functions
- serverM.cpp: communicates with serverS, serverD, and serverU via UDP and client via TCP
- serverS.cpp: keeps and mutates a database of "single" rooms and communicates the data with serverM
- serverD.cpp: keeps and mutates a database of "double" rooms and communicates the data with serverM
- serverU.cpp: keeps and mutates a database of "suite" rooms and communicates the data with serverM
- client.cpp: provides user interface for client to login as member or guest and make Availability and Reservation requests

how information gets passed between servers
- username and password
    - client<->serverM: concatenated and delimited by a comma
- room code and (availability or reservation) request
    client<->serverM<->serverS OR serverD OR serverU: concatenated and delimited by a comma
- login code
    - client<-serverM: cstring representing login code
- availability / reservation request code
    - client<-serverM: cstring representing availability code
- availability request to back end
    - serverM<->serverS OR serverD OR serverU: appended "A" to front of roomcode to signify Availability
- reservation request to back end
    - serverM<->serverS OR serverD OR serverU: appended "R" to front of roomcode to signify Reservation

citations
- I used code from chapter 6 Beej's
    - TCP send and recv in client.cpp and serverM.cpp
    - UDP sendto and recvfrom
- getsockname from the project description
