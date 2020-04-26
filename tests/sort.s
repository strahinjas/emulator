.global sort
.extern digit_sum

# arguments:
# r0 = array's starting address
# r1 = array's size
#
# return:
# void

.text
sort:
	push r2
	push r3
	push r4
	push r5
	
	mov limiti, r1
	sub limiti, 1	# limiti = size - 1
	mov limitj, r1	# limitj = size
	mov r5, r0		# r5 = array
	
	mov r2, 0		# r2 = i
loopi:
	cmp r2, limiti
	jeq exit
	jgt exit
	mov index, r2	# index = i
	mov r3, r2
	add r3, 1		# r3 = j = i + 1
loopj:
	cmp r3, limitj
	jeq break
	jgt break
	
	mov r4, r3
	shl r4, 1
	add r4, r5
	mov r0, [r4]	# r0 = array[j]
	call digit_sum
	mov temp, r0	# temp = first digit sum
	
	mov r4, index
	shl r4, 1
	add r4, r5
	mov r0, [r4]	# r0 = array[index]
	call digit_sum
	
	cmp temp, r0
	jeq next
	jgt next
	mov index, r3	# index = j
next:
	add r3, 1
	jmp loopj
break:
	mov r3, index
	shl r3, 1
	add r3, r5		# r3 = array + index
	mov r4, r2
	shl r4, 1
	add r4, r5		# r4 = array + i
	mov temp, [r3]	# temp = array[index]
	mov [r3], [r4]	# array[index] = array[i]
	mov [r4], temp	# array[i] = temp
	add r2, 1
	jmp loopi
exit:
	pop r5
	pop r4
	pop r3
	pop r2
	ret

.data
limiti:				# limit for i loop
	.word 0
limitj:				# limit for j loop
	.word 0
index:				# minimum index
	.word 0
temp:				# helper location
	.word 0

.end