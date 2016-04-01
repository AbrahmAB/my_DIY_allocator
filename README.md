IT215: SYSTEM SOFTWARE Project 2: DIY allocator
-----------------------------------------------

Use of explicit free list : 
------------------------------

We have used explicit free list to maintain the list of pointers to free blocks. This enhances speed wise performance of allocator as compared to implicit free list. This is because allocator has to traverse only the list of free list instead traversing the whole list of blocks.<br>

Implemented explicit free list to maintain the list of pointers to free blocks.<br>
-> Data structure used is doubly Linked list.<br>
-> Each free-block contain two pointers.<br>
	-> The first points to previous free block.<br>
	-> The second points to next free block.<br>
-> 'free_lits_header' is used as an header to this Linked list of free blocks.<br>
-> Macros Used :<br>
	* To access previous and next free block.<br>
		#define GET_NEXT_FREEP(bp)  (*(char **)(bp + WSIZE))<br>
		#define GET_PREV_FREEP(bp)  (*(char **)(bp))<br>
  	* To set previous and next free block.<br>
		#define SET_NEXT_FREEP(bp, qp) (GET_NEXT_FREEP(bp) = qp)<br>
		#define SET_PREV_FREEP(bp, qp) (GET_PREV_FREEP(bp) = qp)<br>

-> Functions Used :<br>
	* To add insert new free block in free-list.<br>
		insert_free_blk(void *bp)<br>
		remove_free_blk(void *bp)<br>

Improvements in realloc : 
------------------------------
Made necessary improvements in realloc funtion to improve the efficiency.<br>
-> The previously implemented realloc called malloc to find memory of new size and copy all contents to it. This reduced the throughput drastically.<br>
-> Possible solution tried here:<br>
	* Check if the block needs to expand or not.<br>
	* Check the next block is free or not and the combined size of both current block and next is greater than the requested size.<br>
		=> If next block is free then only change the size of current block and set header and footer.<br>
		=> If next block is not free just go with the traditional method of calling malloc to find memory of new size and copy all contents 			to it.<br>

Coalescing : 
---------------
Immediate coalescing using  boundary tag coalescing. <br>

Insertion policy in free list :
---------------------------------
-> LIFO (last-in-first-out) policy<br>
-> Insert freed block at the beginning of the free list<br>
ie., latest block to be freed may be next one to be allocated<br>

Searching policy :
---------------------
Searching through free list executes first fit algorithm.<br>

Development stages:
-------------------
Note: 
distro: Linux Mint 17.3 Cinnamon 64-bit<br>

Trace files (short{1,2}-bal.rep) that used for initial debugging and testing. Once they started yeilding efficient performance (86/100 and 96/100), proceeded further with 9 traces. And for the last two traces made proper changes in realloc as described above.<br>

The overall performance is:<br>
  Pref Index  = 31/40 (util) + 60/60 (thru) = 91/100







