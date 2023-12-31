#	$NetBSD: t_io_assist.sh,v 1.3 2023/08/05 13:05:14 riastradh Exp $
#
# Copyright (c) 2019-2020 Maxime Villard, m00nbsd.net
# All rights reserved.
#
# This code is part of the NVMM hypervisor.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

atf_test_case io_assist
io_assist_head()
{
	atf_set "descr" "Check the I/O Assist provided in libnvmm"
	atf_set "require.user" "root"
}

io_assist_body()
{
	$(atf_get_srcdir)/h_io_assist

	exitcode=$?

	if [ $exitcode -eq 6 ] ; then
		atf_skip "NVMM driver not loaded"
	elif [ $exitcode -ne 0 ] ; then
		atf_fail "I/O Assist failed with errno $exitcode"
	fi
}

atf_init_test_cases()
{
	atf_add_test_case io_assist
}
