# Makefile for alarmd gtk-doc documentation
# Copyright (C) 2006-2007 Nokia Corporation.
# Contact Person: David Weinehall <david.weinehall@nokia.com>
#
# alarmd and libalarm are distributed in the hope that they will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this software; if not, write to the Free
# Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA

all: doc

doc:
	CFLAGS="-I.. `pkg-config glib-2.0 gobject-2.0 gmodule-2.0 dbus-1 gconf-2.0 libxml-2.0 libosso conic --cflags` -DDBUS_API_SUBJECT_TO_CHANGE" LDFLAGS="`pkg-config glib-2.0 gobject-2.0 gmodule-2.0 dbus-1 gconf-2.0 libxml-2.0 libosso conic --libs` ../.libs/action*.o ../.libs/event*.o ../.libs/queue*.o ../.libs/object*.o ../.libs/rpc*.o ../.libs/*objectfactory.o" gtkdoc-scangobj --module=$(PACKAGE) --output-dir=.
	gtkdoc-scan --module=$(PACKAGE) --source-dir=.. --output-dir=.
	gtkdoc-mktmpl --module=$(PACKAGE)
	gtkdoc-mkdb --module=$(PACKAGE) --source-dir=.. --tmpl-dir=tmpl --output-format=xml --main-sgml-file=$(PACKAGE)-docs.sgml --sgml-mode --ignore-files=debian
	gtkdoc-mkhtml $(PACKAGE) $(PACKAGE)-docs.sgml

clean:
	rm -f *.html sgml/* xml/* *.stamp
