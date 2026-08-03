.entry START
.entry START
.entry DATA
.extern EXT
.extern EXT
START: la DATA
call EXT
jmp DONE
call EXT
DONE: hlt
DATA: .dw 42
Test: .db 1
test: .db 2
