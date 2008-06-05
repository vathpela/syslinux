;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; parseconfig.inc
;;
;; Configuration file operations
;;

		section .text
;
; "default" command
;
pc_default:	mov di,default_cmd
		call getline
		mov byte [di-1],0		; null-terminate
		ret

;
; "ontimeout" command
;
pc_ontimeout:	mov di,Ontimeout
		call getline
		sub di,Ontimeout+1		; Don't need final space
		mov [OntimeoutLen],di
		ret

;
; "onerror" command
;
pc_onerror:	mov di,Onerror
		call getline
		sub di,Onerror
		mov [OnerrorLen],di
		ret

;
; "append" command
;
pc_append:      cmp byte [VKernel],0
		ja .vk
                mov di,AppendBuf
		call getline
                sub di,AppendBuf
.app1:		mov [AppendLen],di
		ret
.vk:		mov di,VKernelBuf+vk_append	; "append" command (vkernel)
		call getline
		sub di,VKernelBuf+vk_append
                cmp di,byte 2
                jne .app2
                cmp byte [VKernelBuf+vk_append],'-'
                jne .app2
                xor di,di			; If "append -" -> null string
.app2:		mov [VKernelBuf+vk_appendlen],di
		ret

;
; "ipappend" command (PXELINUX only)
;
%if IS_PXELINUX
pc_ipappend:	call getint
		jc .err
		cmp byte [VKernel],0
		jne .vk
		mov [IPAppend],bl
.err:		ret
.vk:		mov [VKernelBuf+vk_ipappend],bl
		ret
%endif

;
; "localboot" command (PXELINUX, ISOLINUX)
;
%if HAS_LOCALBOOT

pc_localboot:	call getint
		cmp byte [VKernel],0		; ("label" section only)
		je .err
		mov di,VKernelBuf+vk_rname
		xor ax,ax
		mov cx,FILENAME_MAX
		rep stosb			; Null kernel name
%if IS_PXELINUX
		; PXELINUX uses the first 4 bytes of vk_rname for the
		; mangled IP address
		mov [VKernelBuf+vk_rname+5], bx	; Return type
%else
		mov [VKernelBuf+vk_rname+1], bx	; Return type
%endif
.err:		ret

%endif ; HAS_LOCALBOOT

;
; "kernel", "config", ... command
;
pc_kernel:	cmp byte [VKernel],0
		je .err				; ("label" section only)
		mov [VKernelBuf+vk_type],al
		call pc_getline
		mov di,VKernelBuf+vk_rname
		call mangle_name
.err:		ret

;
; "timeout", "totaltimeout" command
;
; N.B. 1/10 s ~ 1.D2162AABh clock ticks
;
pc_timeout:	push ax
		call getint
		pop si
		jc .err
		mov eax,0D2162AABh
		mul ebx				; clock ticks per 1/10 s
		add ebx,edx
		mov [si],ebx
.err:		ret


;
; "totaltimeout" command
;
pc_totaltimeout:

;
; Generic integer variable setting commands:
; "prompt", "implicit"
;
pc_setint16:
		push ax
		call getint
		pop si
		jc .err
		mov [si],bx
.err:		ret

;
; Generic file-processing commands:
; "font", "kbdmap",
;
pc_filecmd:	push ax				; Function to tailcall
		call pc_getline
		mov di,MNameBuf
		call mangle_name
		call searchdir
		jnz .ok
		pop ax				; Drop the successor function
.ok:		ret				; Tailcall if OK, error return

;
; Commands that expect the file to be opened on top of the getc stack.
; "display", "include"
;
pc_opencmd:	push ax				; Function to tailcall
		call pc_getline
		mov di,MNameBuf
		call mangle_name
		call open
		jnz .ok
		pop ax				; Drop the successor function
.ok:		ret				; Tailcall if OK, error return

;
; "include" command (invoked from pc_opencmd)
;
pc_include:	inc word [IncludeLevel]
.err:		ret

;
; "serial" command
;
pc_serial:	call getint
		jc .err
		push bx				; Serial port #
		call skipspace
		jnc .ok
		pop bx
.err:		ret
.ok:
		call ungetc
		call getint
		mov [FlowControl], word 0	; Default to no flow control
		jc .nobaud
.valid_baud:
		push ebx
		call skipspace
		jc .no_flow
		call ungetc
		call getint			; Hardware flow control?
		jnc .valid_flow
.no_flow:
		xor bx,bx			; Default -> no flow control
.valid_flow:
		and bh,0Fh			; FlowIgnore
		shl bh,4
		mov [FlowIgnore],bh
		mov bh,bl
		and bx,0F003h			; Valid bits
		mov [FlowControl],bx
		pop ebx				; Baud rate
		jmp short .parse_baud
.nobaud:
		mov ebx,DEFAULT_BAUD		; No baud rate given
.parse_baud:
		pop di				; Serial port #
		cmp ebx,byte 75
		jb .err				; < 75 baud == bogus
		mov eax,BAUD_DIVISOR
		cdq
		div ebx
		mov [BaudDivisor],ax
		push ax				; Baud rate divisor
		cmp di,3
		ja .port_is_io			; If port > 3 then port is I/O addr
		shl di,1
		mov di,[di+serial_base]		; Get the I/O port from the BIOS
.port_is_io:
		mov [SerialPort],di

		;
		; Begin code to actually set up the serial port
		;
		lea dx,[di+3]			; DX -> LCR
		mov al,83h			; Enable DLAB
		call slow_out

		pop ax				; Divisor
		mov dx,di			; DX -> LS
		call slow_out

		inc dx				; DX -> MS
		mov al,ah
		call slow_out

		mov al,03h			; Disable DLAB
		inc dx				; DX -> LCR
		inc dx
		call slow_out

		in al,dx			; Read back LCR (detect missing hw)
		cmp al,03h			; If nothing here we'll read 00 or FF
		jne .serial_port_bad		; Assume serial port busted
		dec dx
		dec dx				; DX -> IER
		xor al,al			; IRQ disable
		call slow_out

		inc dx				; DX -> FCR/IIR
		mov al,01h
		call slow_out			; Enable FIFOs if present
		in al,dx
		cmp al,0C0h			; FIFOs enabled and usable?
		jae .fifo_ok
		xor ax,ax			; Disable FIFO if unusable
		call slow_out
.fifo_ok:

		inc dx
		inc dx				; DX -> MCR
		in al,dx
		or al,[FlowOutput]		; Assert bits
		call slow_out

		; Show some life
		cmp byte [SerialNotice],0
		je .notfirst
		mov byte [SerialNotice],0

		mov si,syslinux_banner
		call write_serial_str
		mov si,copyright_str
		call write_serial_str
.notfirst:
		ret

.serial_port_bad:
		mov [SerialPort], word 0
		ret

;
; "F"-key command
;
pc_fkey:	push ax
		call pc_getline
		pop di
		call mangle_name		; Mangle file name
		ret

;
; "label" command
;
pc_label:	call commit_vk			; Commit any current vkernel
		mov di,VKernelBuf		; Erase the vkernelbuf for better compression
		mov cx,(vk_size >> 1)
		xor ax,ax
		rep stosw
		call pc_getline
		mov di,VKernelBuf+vk_vname
		call mangle_name		; Mangle virtual name
		mov byte [VKernel],1		; We've seen a "label" statement
		mov si,VKernelBuf+vk_vname	; By default, rname == vname
		; mov di,VKernelBuf+vk_rname	; -- already set
		mov cx,FILENAME_MAX
		rep movsb
                mov si,AppendBuf		; Default append==global append
                mov di,VKernelBuf+vk_append
                mov cx,[AppendLen]
                mov [VKernelBuf+vk_appendlen],cx
                rep movsb
%if IS_PXELINUX					; PXELINUX only
		mov al,[IPAppend]		; Default ipappend==global ipappend
		mov [VKernelBuf+vk_ipappend],al
%endif
		ret

;
; "say" command
;
pc_say:		call pc_getline			; "say" command
		call writestr
		jmp crlf			; tailcall

;
; "text" command; ignore everything until we get an "endtext" line
;
pc_text:	call pc_getline			; Ignore rest of line
.loop:
		call pc_getline
		jc .eof

		; Leading spaces are already removed...
		lodsd
		and eax,0xdfdfdfdf		; Upper case
		cmp eax,'ENDT'
		jne .loop
		lodsd
		and eax,0x00dfdfdf		; Upper case and mask
		cmp eax,'EXT'
		jne .loop
		; If we get here we hit ENDTEXT
.eof:
		ret

;
; Comment line
;
pc_comment:	; Fall into pc_getline

;
; Common subroutine: load line into trackbuf; returns with SI -> trackbuf
; CF is set on EOF.
;
pc_getline:	mov di,trackbuf
		push di
		call getline
		mov byte [di],0			; Null-terminate
		pop si
		ret

;
; Main loop for configuration file parsing
;
parse_config:
		mov di,VKernelBuf		; Clear VKernelBuf at start
		xor ax,ax
		mov cx,vk_size
		rep stosb

.again:
		call getcommand			; Parse one command
                jnc .again			; If not EOF...
		call close
		dec word [IncludeLevel]		; Still parsing?
		jnz .again

		;
		; The fall through to commit_vk to commit any final
		; VKernel being read
		;
;
; commit_vk: Store the current VKernelBuf into buffer segment
;
commit_vk:
		; For better compression, clean up the append field
		mov ax,[VKernelBuf+vk_appendlen]
		mov di,VKernelBuf+vk_append
		add di,ax
		mov cx,max_cmd_len+1
		sub cx,ax
		xor ax,ax
		rep stosb

		; Pack into high memory
		mov si,VKernelBuf
		mov edi,[VKernelEnd]
		mov cx,vk_size
		call rllpack
		mov [VKernelEnd],edi
		ret
.overflow:
		mov si,vk_overflow_msg
		call writestr
		ret

		section .data
vk_overflow_msg	db 'Out of memory parsing config file', CR, LF, 0
SerialNotice	db 1			; Only print this once

		section .bss
		alignb 4
VKernelEnd	resd 1			; Lowest high memory address used

		; This symbol should be used by loaders to indicate
		; the highest address *they* are allowed to use.
HighMemRsvd	equ VKernelEnd
					; by vkernels
		section .config
		align 4, db 0
KbdTimeout      dd 0                    ; Keyboard timeout (if any)
TotalTimeout	dd 0			; Total timeout (if any)
AppendLen       dw 0                    ; Bytes in append= command
OntimeoutLen	dw 0			; Bytes in ontimeout command
OnerrorLen	dw 0			; Bytes in onerror command
CmdLinePtr	dw cmd_line_here	; Command line advancing pointer
ForcePrompt	dw 0			; Force prompt
NoEscape	dw 0			; No escape
AllowImplicit   dw 1                    ; Allow implicit kernels
AllowOptions	dw 1			; User-specified options allowed
IncludeLevel	dw 1			; Nesting level
SerialPort	dw 0			; Serial port base (or 0 for no serial port)
VKernel		db 0			; Have we seen any "label" statements?

%if IS_PXELINUX
IPAppend	db 0			; Default IPAPPEND option
%endif

		section .uibss
                alignb 4		; For the good of REP MOVSD
command_line	resb max_cmd_len+2	; Command line buffer
		alignb 4
default_cmd	resb max_cmd_len+1	; "default" command line

%include "rllpack.inc"