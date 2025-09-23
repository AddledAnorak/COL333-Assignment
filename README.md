# COL333-Assignment

Commit 429d1fd004722f18a13bcb2ee3da2f8ed5069ff2: 100 moves ~ 1-5s (for both mcts kinds)
    mcts: written by me initially
    mcts_optimized: edited and optimized (uses nodepool but cant handle too much memory alloc)


Commit 5eccd0022f6516531eb07774229bf65edfa99f35: 1000 moves ~ O(1) secs
    10k moves ~ 200s for move 1
    time increases with depth & breadth (need to analyze properly)

    main improvements:
        improved game engine, 100% sure its more optimized now