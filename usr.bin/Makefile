#	@(#)Makefile	8.3 (Berkeley) 1/7/94

SUBDIR=	apply apropos ar banner basename bdes biff cal calendar \
	cap_mkdb checknr chflags chpass cksum col colcrt colrm column \
	comm cpp ctags cut diff dirname du \
	env error expand false find finger fmt fold fpr from \
	fsplit fstat ftp gcore gprof grep head hexdump id indent \
	join jot kdump ktrace lam last lastcomm leave locate \
	lock logger login logname lorder m4 mail make man mesg mkdep \
	mkfifo mklocale mkstr more msgs mt netstat nfsstat nice nm nohup \
	pagesize passwd paste patch pr printenv printf quota \
	ranlib rdist renice rev rlogin rs rsh ruptime rwho sccs \
	script sed shar showmount size soelim split \
	strings strip su talk tcopy tee telnet tftp time tip \
	touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq unvis users uudecode uuencode vacation \
	vis w wall wc what whatis whereis who whois window write \
	xargs xinstall xstr yacc yes

# Missing on 4.4bsd tape:
# at bc compress dc deroff diction graph learn
# plot ptx spell spline struct units xsend

# TODO: ex file sort uucp

# Cmp, look and tail all use mmap, so new-VM only.
.if	${MACHINE} == "hp300"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "i386"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "luna68k"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "mips"
SUBDIR+=cmp look systat tail vmstat
.elif	${MACHINE} == "sparc"
SUBDIR+=cmp ld look tail vmstat.sparc
.elif	${MACHINE} == "tahoe"
SUBDIR+=systat vmstat
.elif	${MACHINE} == "vax"
SUBDIR+=systat vmstat
.endif

.include <bsd.subdir.mk>
