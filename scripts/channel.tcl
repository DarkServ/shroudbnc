# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005 Gunnar Beutner
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

internaltimer 120 1 sbnc:channelflush
internalbind unload sbnc:channelflush
internalbind usrdelete sbnc:channelconfdelete

proc sbnc:channelflush {} {
	foreach user [bncuserlist] {
		setctx $user
		savechannels

		foreach channel [channels] {
			if {![botonchan $channel] && ![channel get $channel inactive]} {
				# simul so we can take advantage of keyrings
				simul [getctx] "JOIN $channel"
			}
		}
	}
}

proc sbnc:channelconfdelete {client} {
	file delete "users/$client.chan"
}

proc channel {option chan args} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::channels channels
	upvar [getns]::chanoptions chanoptions

	if {[botonchan $chan] && ![info exists channels($chan)]} {
		set channels($chan) ""
	}

	if {[info exists channels($chan)]} {
		array set channel $channels($chan)
	} else {
		array set channel {}
	}

	switch [string tolower $option] {
		add {
			set channels([string tolower $chan]) [join $args]

			simul [getctx] "JOIN $chan"

			return 1
		}
		set {
			if {[llength $args] < 1} { return -code error "Too few parameters" }

			set first [string index [lindex $args 0] 0]

			if {$first == "+" || $first == "-"} {
				set option [string range [lindex $args 0] 1 end]
			} else {
				set option [lindex $args 0]
			}

			set value [lindex $args 1]

			if {![info exists chanoptions($option)]} {
				return -code error "No such option."
			} elseif {[string equal -nocase $chanoptions($option) "int"]} {
				if {[llength $args] < 2} { return -code error "Too few parameters" }
				elseif {![string is digit $value]} { return -code error "Value is not an integer." }

				set channel($option) $value
			} elseif {$first == "+" || $first == "-"} {
				if {![string equal -nocase $chanoptions($option) "flag"]} { return -code error "Value is not a flag." }

				if {$first == "+"} {
					set channel($option) 1
				} else {
					set channel($option) 0
				}
			} elseif {![validchan [lindex $args 0]]} {
				return -code error "no such channel record"
			} else {
				set channel($option) $value
			}

			set channels([string tolower $chan]) [array get channel]

			return [lindex $args 1]
		}
		info {
			return $channels($chan)
		}
		get {
			if {[llength $args] < 1} { return -code error "Too few parameters" }

			if {![info exists chanoptions([lindex $args 0])]} {
				return -code error "No such option."
			} elseif {[info exists channel([lindex $args 0])]} {
				return $channel([lindex $args 0])
			} else {
				if {$chanoptions(inactive) == "int" || $chanoptions(inactive) == "flag"} {
					return 0
				} else {
					return {}
				}
			}
		}
		remove {
			if {[info exists channels($chan)]} {
				unset channels([string tolower $chan])
			}

			puthelp "PART $chan"

			return 1
		}
		default {
			return -code error "Option should be one of: add set info get remove"
		}
	}
}

proc savechannels {} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
	}

	upvar [getns]::channels channels

	set file [open $::chanfile "w"]

	foreach channel [array names channels] {
		puts $file "channel add $channel \{ $channels($channel) \}"
	}

	close $file

	return 1
}

proc loadchannels {} {
	namespace eval [getns] {
		array set channels {}
	}

	catch [list source $::chanfile]

	return
}

proc channels {} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
	}

	upvar [getns]::channels channels

	set tmpchans [array names channels]

	foreach chan [internalchannels] {
		lappend tmpchans $chan
	}

	return [sbnc:uniq $tmpchans]
}

proc validchan {channel} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
	}

	upvar [getns]::channels channels

	if {[info exists channels([string tolower $channel])]} {
		return 1
	} else {
		return 0
	}
}

proc isdynamic {channel} {
	return [validchan $channel]
}

proc setudef {type name} {
	namespace eval [getns] {
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::chanoptions chanoptions

	set chanoptions($name) $type

	return
}

proc renudef {type oldname newname} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::chanoptions chanoptions
	upvar [getns]::channels channels

	if {[info exists chanoptions($newname)]} {
		return -code error "$newname is already a channel option."
	}

	if {[info exists chanoptions($oldname)]} {
		set chanoptions($newname) $chanoptions($oldname)
		unset chanoptions($oldname)
	} else {
		setudef $type $newname
	}

	foreach channame [array names channels] {
		array set channel $channels($channame)

		if {[info exists channel($oldname)]} {
			set channel($newname) $channel($oldname)
			unset channel($oldname)
		}

		set channels($channame) [array get channel]
	}

	return
}

proc deludef {type name} {
	namespace eval [getns] {
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::chanoptions chanoptions

	if {[info exists chanoptions($name)]} {
		unset chanoptions($name)
	}

	return
}

if {![info exists sbnc:channelinit]} {
	foreach user [bncuserlist] {
		setctx $user
		loadchannels
	}

	set sbnc:channelinit 1
}

setudef int inactive
