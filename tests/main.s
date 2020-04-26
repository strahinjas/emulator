.global _start

# this is not necessary
# but recommended
.extern size
.extern array
.extern print
.extern sort

.text
_start:
	mov r0, &array
	mov r1, &size
	call print
	
	mov r0, &array
	mov r1, &size
	call sort
	
	mov r0, &array
	mov r1, &size
	call print
	
	halt

.end