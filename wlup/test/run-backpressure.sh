#!/bin/bash
# Backpressure test of wlup, run inside the xrdp-wlup-dev image:
#
#   xfreerdp3 (in Xvfb) --RDP over lo, shaped by tc netem--> xrdp + libwlup
#       --Wayland--> headless sway
#
# Does wlup send data nobody needs, and does a busy screen on a slow link
# pile up frames that delay everything after them? xrdp queues whatever
# the socket does not take in user space (trans_write_copy_s(), no size
# limit), so overproduction does not show as more bytes on the wire: it
# shows as a growing delay before the client sees a change, and as a
# growing xrdp process. This test measures those, for each way frames can
# go to the client (BP_PATHS):
#
#   h264     /bpp:32 /gfx:AVC420  wlup -> GFX H.264 encoder, paced by acks
#   gfx-rfx  /bpp:32 /gfx:RFX     wlup -> server_paint_rect() -> GFX planar
#   bitmap   /bpp:24 -gfx         wlup -> server_paint_rect() -> bitmap updates
#
# Scenarios, on one connection per path:
#
#   S1 idle      a still screen: bytes per second
#   S2 busy      a terminal printing random text fills half the screen,
#                full speed link: bytes per second, CPU, probe delay
#   S3 slow      the same on a slow link (netem, BP_RATE and BP_DELAY each
#                way): probe delay over BP_SLOW_S seconds, and whether it
#                grows; xrdp RSS growth; the kernel send queue
#   S3r recover  link back to full speed: delay of the next probe, which
#                is long if xrdp had a backlog queued
#   S4 hidden    the client window is unmapped while the screen stays
#                busy: xfreerdp sends Suppress Output; bytes per second and
#                CPU (xrdp process, which runs wlup, and sway)
#   S5 resume    the probe colour changes while hidden, then the window is
#                mapped again: time until the client shows the new colour
#
# A probe: typing 'pb1' .. 'pb5' in a foot terminal runs a script that sets
# the terminal background to one of five colours (OSC 11), the next one
# each time, so a stale frame cannot pass for a new one, and writes a
# timestamp.
# Keystroke to timestamp is the input delay; timestamp to the colour
# showing in the client window (read back from Xvfb) is the output delay,
# the one a backlog makes longer. Reading a pixel from Xvfb takes some
# time itself: S2 shows that floor.
#
# Needs CAP_NET_ADMIN for tc, and the host's sch_netem module:
#
#   docker run --rm --cap-add NET_ADMIN -v <xrdp src>:/src:ro -v <out>:/out \
#       xrdp-wlup-dev bash /src/wlup/test/run-backpressure.sh /src /out
#
# Results: <out>/summary.txt, and per path <out>/<path>-monitor.csv
# (twice a second: bytes acknowledged, send queue, xrdp RSS and CPU) and
# <out>/<path>-probes.csv.
#
# Usage: run-backpressure.sh <xrdp source dir> <output dir>
set -eu

SRC=$1
OUT=$2
W=1280
H=800
BP_PATHS=${BP_PATHS:-"h264 gfx-rfx bitmap"}
BP_RATE=${BP_RATE:-4mbit}
BP_DELAY=${BP_DELAY:-80ms}
BP_SLOW_S=${BP_SLOW_S:-30}
PROBE_TIMEOUT_MS=20000
CLK_TCK=$(getconf CLK_TCK)

mkdir -p "$OUT"

# --- build and install xrdp --------------------------------------------------
cp -a "$SRC" /build
cd /build
./bootstrap > "$OUT/bootstrap.log" 2>&1
# -Wno-error=nonnull: GCC 16 flags existing code in xrdp/xrdp_mm.c
./configure --enable-wlup --enable-x264 --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
    CFLAGS="-O2 -g -Wno-error=nonnull" > "$OUT/configure.log" 2>&1
make -j"$(nproc)" > "$OUT/make.log" 2>&1
make install > "$OUT/install.log" 2>&1
ldconfig

# --- the slow link -----------------------------------------------------------
# netem on lo delays and rate-limits both directions. A real link has a
# small buffer: 'limit' keeps netem's own queue to 100 packets (~150 KB,
# 300 ms at 4 Mbit), so a growing delay comes from xrdp, not from netem.
# That needs real packets: lo gets a real MTU, and no GSO, which would
# otherwise hand netem 64 KB packets and let it hold megabytes. Both are
# set before any connection exists, as TCP picks its segment size when
# connecting
ip link set lo mtu 1500
ip link set lo gso_max_segs 1 gso_max_size 1500
SHAPE=0
if tc qdisc add dev lo root netem delay 1ms 2> "$OUT/tc.log" &&
        tc qdisc del dev lo root 2>> "$OUT/tc.log"; then
    SHAPE=1
fi
link_slow() {
    tc qdisc add dev lo root netem delay "$BP_DELAY" rate "$BP_RATE" limit 100
}
link_fast() {
    tc qdisc del dev lo root 2> /dev/null || true
}

# --- the test user, their sway config and the probe ----------------------------
echo 'tester:wlup-test-pw' | chpasswd
mkdir -p /home/tester/.config/sway
cat > /home/tester/.config/sway/config <<'SWAYCONF'
output * bg #204a87 solid_color
focus_follows_mouse no
default_border none
exec foot -a probe
SWAYCONF
chown -R tester /home/tester/.config
# Clears the screen, so the probed pixel never has text on it, and sets
# the background colour
for p in pb1:ff0000 pb2:00ff00 pb3:ffff00 pb4:ff00ff pb5:00ffff; do
    cat > /usr/local/bin/${p%%:*} <<EOF
#!/bin/sh
printf '\033[H\033[2J\033]11;#${p#*:}\007'
date +%s%N > /tmp/probe-ran
EOF
    chmod 755 /usr/local/bin/${p%%:*}
done

SWAYSOCK=
swaymsg_() {
    su tester -c "swaymsg -s $SWAYSOCK $(printf '%q ' "$@")"
}

# --- xrdp ---------------------------------------------------------------------
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=wlup-test \
    -keyout /etc/xrdp/key.pem -out /etc/xrdp/cert.pem > /dev/null 2>&1
cat >> /etc/xrdp/xrdp.ini <<EOF

[Wayland]
name=Wayland (wlup prototype)
lib=libwlup.so
username=ask
password=ask
port=-1
code=30
xkb_layout=us
EOF
sed -i 's/^#\?autorun=.*/autorun=Wayland/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/' /etc/xrdp/xrdp.ini
sed -i 's/^LogLevel=.*/LogLevel=DEBUG/' /etc/xrdp/sesman.ini
xrdp-sesman --nodaemon > "$OUT/sesman.log" 2>&1 &
xrdp --nodaemon > "$OUT/xrdp.log" 2>&1 &
sleep 2

Xvfb :99 -screen 0 1920x1080x24 > /dev/null 2>&1 &
export DISPLAY=:99
sleep 1

# Measuring goes on through failures
set +e

# --- measuring -----------------------------------------------------------------
now_ms() {
    echo $(($(date +%s%N) / 1000000))
}
# The connection's bytes acknowledged by the client, and its send queue
tcp_stats() {
    ss -tniH state established '( sport = :3389 )' 2> /dev/null | tr '\n' ' ' |
        awk '{ acked = 0; notsent = 0
               if (match($0, /bytes_acked:[0-9]+/))
                   acked = substr($0, RSTART + 12, RLENGTH - 12)
               if (match($0, /notsent:[0-9]+/))
                   notsent = substr($0, RSTART + 8, RLENGTH - 8)
               print acked + 0, $2 + 0, notsent + 0 }'
}
# xrdp forks a process per connection, which loads libwlup
xrdp_pid() {
    pgrep -n -x xrdp
}
rss_kb() {
    awk '/^VmRSS/ { print $2 }' /proc/"$1"/status 2> /dev/null || echo 0
}
cpu_ticks() {
    awk '{ print $14 + $15 }' /proc/"$1"/stat 2> /dev/null || echo 0
}
# A point in time: ms, bytes acked, xrdp RSS, xrdp CPU ticks, sway CPU ticks
snap() {
    local acked rest
    read -r acked rest < <(tcp_stats)
    echo "$(now_ms) ${acked:-0} $(rss_kb "$XPID") $(cpu_ticks "$XPID") $(cpu_ticks "$SWAY_PID")"
}
# Between two snaps: "KB/s xrdp-CPU% sway-CPU%"
rates() {
    echo "$1 $2" | awk -v hz="$CLK_TCK" '{ ms = $6 - $1; if (ms < 1) ms = 1
        printf "%.1f %.0f %.0f\n", ($7 - $2) / 1.024 / ms,
            ($9 - $4) * 100000 / hz / ms, ($10 - $5) * 100000 / hz / ms }'
}
monitor() {
    echo "t_ms,bytes_acked,send_q,notsent,xrdp_rss_kb,xrdp_cpu_ticks,sway_cpu_ticks" > "$1"
    while :; do
        read -r acked sendq notsent < <(tcp_stats)
        echo "$(now_ms),${acked:-0},${sendq:-0},${notsent:-0},$(rss_kb "$XPID"),$(cpu_ticks "$XPID"),$(cpu_ticks "$SWAY_PID")" >> "$1"
        sleep 0.5
    done
}
# Colour of a pixel of the client window (in Xvfb) as "r g b"
pixel() {
    import -window root -crop "1x1+$1+$2" -depth 8 txt:- 2> /dev/null |
        sed -n '2s/^[^(]*(\([0-9.]*\),\([0-9.]*\),\([0-9.]*\).*/\1 \2 \3/p'
}
# is_colour <rgb mask, e.g. 110 = yellow> r g b: each channel high (> 160)
# or low (< 90), as H.264 is lossy
is_colour() {
    local i v
    local -a c=("${2%.*}" "${3%.*}" "${4%.*}")
    for i in 0 1 2; do
        v=${c[$i]}
        [ -n "$v" ] || return 1
        if [ "${1:$i:1}" = 1 ]; then
            [ "$v" -gt 160 ] || return 1
        else
            [ "$v" -lt 90 ] || return 1
        fi
    done
}
# wait_colour <colour> <deadline ms>: return 1 on timeout
wait_colour() {
    while ! is_colour "$1" $(pixel "$PX" "$PY"); do
        [ "$(now_ms)" -gt "$2" ] && return 1
    done
    return 0
}
# The probed pixel: low in the probe terminal, below its text
probe_point() {
    read -r PX PY < <(swaymsg_ -t get_tree |
        jq -r '.. | objects | select(.app_id? == "probe") |
            "\(.rect.x + .rect.width * 3 / 4 | floor) \(.rect.y + .rect.height * 3 / 4 | floor)"')
}
# Focuses the probe terminal in sway and points the client's pointer at
# it, above the probed pixel
focus_probe() {
    swaymsg_ '[app_id=probe]' focus > /dev/null
    sleep 0.3
    probe_point
    xdotool mousemove "$PX" $((PY - 100)) click 1
    sleep 0.3
}
# probe: types the next probe command, appends "t_ms input_ms output_ms"
# to $PROBES (-1: timed out) and echoes the output delay
MASKS=(100 010 110 101 011)
NEXT=0
probe() {
    local cmd mask before t0 t1 t2 deadline
    cmd=pb$((NEXT + 1))
    mask=${MASKS[$NEXT]}
    NEXT=$(((NEXT + 1) % 5))
    before=$(cat /tmp/probe-ran 2> /dev/null)
    xdotool type --delay 20 "$cmd"
    t0=$(date +%s%N)
    xdotool key Return
    deadline=$((t0 / 1000000 + PROBE_TIMEOUT_MS))
    while [ "$(cat /tmp/probe-ran 2> /dev/null)" = "$before" ]; do
        if [ "$(now_ms)" -gt $deadline ]; then
            echo "$((t0 / 1000000)),-1,-1" >> "$PROBES"
            echo -1
            return
        fi
        sleep 0.02
    done
    t1=$(cat /tmp/probe-ran)
    if wait_colour $mask $deadline; then
        t2=$(date +%s%N)
        echo "$((t0 / 1000000)),$(((t1 - t0) / 1000000)),$(((t2 - t1) / 1000000))" >> "$PROBES"
        echo $(((t2 - t1) / 1000000))
    else
        echo "$((t0 / 1000000)),$(((t1 - t0) / 1000000)),-1" >> "$PROBES"
        echo -1
    fi
}
# Percentile of the numbers on stdin, -1 (timeouts) counted as the
# largest: pct <p>
pct() {
    awk '{ v = ($1 < 0) ? 999999 : $1; print v }' | sort -n |
        awk -v p="$1" '{ a[NR] = $1 } END { if (NR == 0) { print "n/a"; exit }
            i = int((NR - 1) * p / 100) + 1; print a[i] }'
}
busy_start() {
    swaymsg_ exec "foot -a busy base64 -w 150 /dev/urandom" > /dev/null
    sleep 2
    focus_probe
}
busy_stop() {
    swaymsg_ '[app_id=busy]' kill > /dev/null
    sleep 1
}

# --- one path ---------------------------------------------------------------
# run_path <name> <client options...>
run_path() {
    local name=$1 wid log_from s0 s1 lat r_idle r_busy r_slow r_hidden
    local fast_p50 slow_p50 slow_p90 slow_max first_med last_med rss10 rss_end
    local sendq_max recover_ms suppressed resume_ms mode
    shift
    PROBES=$OUT/$name-probes.csv
    echo "t_ms,input_ms,output_ms" > "$PROBES"
    log_from=$(($(wc -l < /var/log/xrdp.log) + 1))

    echo "=== $name: connecting ($*)"
    ${XFREERDP:-xfreerdp3} /v:127.0.0.1 /u:tester /p:wlup-test-pw /cert:ignore \
        /size:${W}x$H -grab-keyboard "$@" > "$OUT/$name-client.log" 2>&1 &
    CLIENT=$!
    sleep 8
    SWAYSOCK=$(find /run /tmp -name 'sway-ipc.*.sock' 2> /dev/null | head -1)
    SWAY_PID=$(pgrep -u tester -x sway)
    XPID=$(xrdp_pid)
    wid=$(xdotool search --pid $CLIENT | tail -1)
    monitor "$OUT/$name-monitor.csv" &
    MON=$!
    focus_probe
    probe > /dev/null    # sets a known colour
    probe > /dev/null
    import -window root "$OUT/$name-1-connected.png"

    # S1: idle
    s0=$(snap); sleep 10; s1=$(snap)
    r_idle=$(rates "$s0" "$s1")
    echo "S1 idle: KB/s xrdp% sway% = $r_idle"

    # S2: busy, fast link
    busy_start
    sleep 3
    s0=$(snap)
    for _ in 1 2 3 4 5; do probe; sleep 1; done > "$OUT/$name-s2.lat"
    sleep 3
    s1=$(snap)
    r_busy=$(rates "$s0" "$s1")
    fast_p50=$(pct 50 < "$OUT/$name-s2.lat")
    import -window root "$OUT/$name-2-busy.png"
    echo "S2 busy: KB/s xrdp% sway% = $r_busy, probe delay p50 $fast_p50 ms"

    # S3: busy, slow link
    if [ $SHAPE = 1 ]; then
        link_slow
        sleep 2
        local t_start t_end
        t_start=$(now_ms)
        s0=$(snap)
        rss10=
        : > "$OUT/$name-s3.lat"
        while [ $(($(now_ms) - t_start)) -lt $((BP_SLOW_S * 1000)) ]; do
            probe >> "$OUT/$name-s3.lat"
            if [ -z "$rss10" ] && [ $(($(now_ms) - t_start)) -ge 10000 ]; then
                rss10=$(rss_kb "$XPID")
            fi
            sleep 0.5
        done
        s1=$(snap)
        t_end=$(now_ms)
        rss_end=$(rss_kb "$XPID")
        r_slow=$(rates "$s0" "$s1")
        slow_p50=$(pct 50 < "$OUT/$name-s3.lat")
        slow_p90=$(pct 90 < "$OUT/$name-s3.lat")
        slow_max=$(pct 100 < "$OUT/$name-s3.lat")
        # first and last third of the probes
        local n
        n=$(wc -l < "$OUT/$name-s3.lat")
        first_med=$(head -n $(((n + 2) / 3)) "$OUT/$name-s3.lat" | pct 50)
        last_med=$(tail -n $(((n + 2) / 3)) "$OUT/$name-s3.lat" | pct 50)
        sendq_max=$(awk -F, -v a="$t_start" -v b="$t_end" \
            'NR > 1 && $1 >= a && $1 <= b && $3 > m { m = $3 } END { print m + 0 }' \
            "$OUT/$name-monitor.csv")
        import -window root "$OUT/$name-3-slow.png"
        link_fast
        # S3r: the first probe on the restored link waits for any backlog
        probe > "$OUT/$name-s3r.lat"
        recover_ms=$(cat "$OUT/$name-s3r.lat")
        echo "S3 slow: KB/s xrdp% sway% = $r_slow, probe delay p50 $slow_p50" \
             "p90 $slow_p90 max $slow_max ms (first third $first_med, last" \
             "third $last_med), RSS ${rss10:-?} -> $rss_end KB, send queue max" \
             "$sendq_max B, after the link is restored $recover_ms ms"
    fi

    # S4: hidden
    local sup_from
    sup_from=$(wc -l < /var/log/xrdp.log)
    xdotool windowunmap "$wid"
    sleep 2
    s0=$(snap); sleep 10; s1=$(snap)
    r_hidden=$(rates "$s0" "$s1")
    suppressed=0
    tail -n +$((sup_from + 1)) /var/log/xrdp.log | grep -q "wlup: output suppressed" &&
        suppressed=1
    echo "S4 hidden: Suppress Output received $suppressed, KB/s xrdp% sway% = $r_hidden"

    # S5: resume, with a colour the client has not seen
    local fpid sh t0
    fpid=$(swaymsg_ -t get_tree |
        jq -r '.. | objects | select(.app_id? == "probe") | .pid')
    sh=$(pgrep -P "$fpid" | head -1)
    # Written to the terminal's pty: /proc/<pid>/fd needs CAP_SYS_PTRACE
    printf '\033[H\033[2J\033]11;#0000ff\007' > /dev/"$(ps -o tty= -p "$sh" | tr -d ' ')"
    sleep 1
    t0=$(now_ms)
    xdotool windowmap "$wid"
    if wait_colour 001 $((t0 + 10000)); then
        resume_ms=$(($(now_ms) - t0))
    else
        resume_ms=-1
    fi
    import -window root "$OUT/$name-5-resumed.png"
    echo "S5 resume: new colour shown after $resume_ms ms"

    busy_stop
    kill $MON 2> /dev/null
    kill $CLIENT 2> /dev/null
    sleep 3
    tail -n +$log_from /var/log/xrdp.log > "$OUT/$name-xrdp.log"
    if grep -q "frames go to the GFX H.264" "$OUT/$name-xrdp.log"; then
        mode=h264
    else
        mode=paint_rect
    fi

    # name mode idle busy slow hidden(KB/s xrdp% sway% each) fast_p50
    # slow_p50 slow_p90 slow_max first_med last_med rss10 rss_end sendq_max
    # recover suppressed resume
    echo "$name $mode $r_idle $r_busy ${r_slow:-- - -} $r_hidden $fast_p50" \
         "${slow_p50:--} ${slow_p90:--} ${slow_max:--} ${first_med:--}" \
         "${last_med:--} ${rss10:--} ${rss_end:--} ${sendq_max:--}" \
         "${recover_ms:--} $suppressed $resume_ms" >> "$OUT/results.txt"
}

: > "$OUT/results.txt"
for p in $BP_PATHS; do
    case $p in
        h264)    run_path h264 /bpp:32 /gfx:AVC420 ;;
        gfx-rfx) run_path gfx-rfx /bpp:32 /gfx:RFX ;;
        bitmap)  run_path bitmap /bpp:24 -gfx ;;
    esac
done

# --- verdicts -----------------------------------------------------------------
pass=0
fail=0
skip=0
check() {
    if [ "$2" = skip ]; then
        echo "SKIP  $1"; skip=$((skip + 1))
    elif eval "$2"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1"; fail=$((fail + 1))
    fi
}
lt() { # lt <a> <b>: a < b, for decimals; n/a and - are false
    awk -v a="$1" -v b="$2" 'BEGIN { exit !(a ~ /^-?[0-9.]+$/ && a + 0 < b + 0) }'
}
{
if [ $SHAPE = 1 ]; then
    echo "slow link: $BP_RATE and $BP_DELAY each way (netem on lo)"
else
    echo "slow link: NOT AVAILABLE (tc netem failed, see tc.log)"
fi
echo
printf '%-8s %-10s %8s %8s %8s %8s  %s\n' path frames idle busy slow hidden "KB/s"
while read -r name mode i1 i2 i3 b1 b2 b3 s1 s2 s3 h1 h2 h3 rest; do
    printf '%-8s %-10s %8s %8s %8s %8s\n' "$name" "$mode" "$i1" "$b1" "$s1" "$h1"
done < "$OUT/results.txt"
echo
printf '%-8s %8s %8s %8s  %s\n' path busy slow hidden "CPU % of the xrdp process (wlup) / sway"
while read -r name mode i1 i2 i3 b1 b2 b3 s1 s2 s3 h1 h2 h3 rest; do
    printf '%-8s %8s %8s %8s\n' "$name" "$b2/$b3" "$s2/$s3" "$h2/$h3"
done < "$OUT/results.txt"
echo
printf '%-8s %6s %6s %6s %6s %6s %6s %8s %9s %8s\n' path fast slow50 slow90 max 1st3rd \
    last3rd recover rss_MB sendq_KB
while read -r name mode i1 i2 i3 b1 b2 b3 s1 s2 s3 h1 h2 h3 fp sp50 sp90 smax fm lm \
        r10 rend sq rec sup res; do
    printf '%-8s %6s %6s %6s %6s %6s %6s %8s %9s %8s\n' "$name" "$fp" "$sp50" "$sp90" \
        "$smax" "$fm" "$lm" "$rec" "$((${r10/-/0} / 1024))->$((${rend/-/0} / 1024))" \
        "$((${sq/-/0} / 1024))"
done < "$OUT/results.txt"
echo "(probe delays in ms, keystroke result to client screen; -1 / 999999 = timed out)"
echo
while read -r name mode i1 i2 i3 b1 b2 b3 s1 s2 s3 h1 h2 h3 fp sp50 sp90 smax fm lm \
        r10 rend sq rec sup res; do
    [ "$name" = h264 ] && check "$name: frames go to the H.264 encoder" '[ "$mode" = h264 ]'
    check "$name: S1 idle screen sends < 5 KB/s" 'lt "$i1" 5'
    if [ $SHAPE = 1 ]; then
        check "$name: S3 slow link, probe delay p90 < 2000 ms" 'lt "$sp90" 2000'
        check "$name: S3 slow link, delay does not grow (last third <= 1.5 x first third + 300 ms)" \
            'lt "$lm" "$(awk -v f="$fm" "BEGIN { print f * 1.5 + 300 }")"'
        check "$name: S3 slow link, xrdp grows < 20 MB from 10 s to the end" \
            'lt "$(((rend - r10) / 1024))" 20'
        check "$name: S3r first probe after the link is restored < 2000 ms" 'lt "$rec" 2000'
    else
        check "$name: S3 slow link" skip
    fi
    if [ "$sup" = 1 ]; then
        check "$name: S4 hidden client gets < 5 KB/s" 'lt "$h1" 5'
        check "$name: S4 hidden, wlup's process does < half its busy CPU work" \
            'lt "$h2" "$(awk -v b="$b2" "BEGIN { print b / 2 + 1 }")"'
    else
        check "$name: S4 Suppress Output (not received from the client)" skip
    fi
    check "$name: S5 resumed client shows the current screen < 3000 ms" \
        '[ "$res" -ge 0 ] && lt "$res" 3000'
done < "$OUT/results.txt"
echo "passed $pass, failed $fail, skipped $skip"
} | tee "$OUT/summary.txt"

cp /var/log/xrdp.log "$OUT/xrdp-file.log" 2> /dev/null
chmod -R a+r "$OUT"
