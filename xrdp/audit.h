/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Copyright (C) Osirium Ltd 2010

*/
#define DO_AUDIT    1

#ifdef DO_AUDIT

int APP_CC
xrdp_audit(struct xrdp_process *process, const char* action, const char* message);

#define AUDIT_OPEN( process, message ) xrdp_audit(process, "OPEN", message)
#define AUDIT_CLOSE( process, message ) xrdp_audit(process, "CLOSE", message)
#define AUDIT_FAILED_OPEN( process, message ) xrdp_audit(process, "FAILED_OPEN", message)

#else

#define AUDIT_CONNECT( process, message )
#define AUDIT_DISCONNECT( process, message )
#define AUDIT_FAILED( process, message )

#endif
