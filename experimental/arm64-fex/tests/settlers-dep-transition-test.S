.text
.globl _try_code
_try_code:
    pushl %ebp
    movl %esp,%ebp
    movl %esp,_resume_esp
    movl $1f,_resume_ip
    movl 8(%ebp),%eax
    call *%eax
1:
    popl %ebp
    ret
