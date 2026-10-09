#!/bin/bash
# Shared Bash 3.2/MSYS2 scheduler; source after defining case_cost/case_selected.
# A worker owns its engine child, including on an interrupted suite run.
run_binary() {
	"$BIN" "$@" &
	local binary_pid=$! rc
	trap 'kill "$binary_pid" 2>/dev/null; wait "$binary_pid" 2>/dev/null; exit 130' INT TERM
	wait "$binary_pid"
	rc=$?
	trap - INT TERM
	return "$rc"
}

# Declaration order is separate from execution order. Arguments are escaped by
# Bash itself, so labels/paths containing spaces remain a single argument.
case_commands=()
case_labels=()
case_order=()
case_count=0
queued_labels=""
active_pids=('')
active_cases=()
queue_dir=$(mktemp -d "$ACTUAL_DIR/.queue.XXXXXX") || exit 1

cleanup_queue() {
	local pid
	for pid in "${active_pids[@]}"; do
		[ -z "$pid" ] || kill "$pid" 2>/dev/null
	done
	for pid in "${active_pids[@]}"; do
		[ -z "$pid" ] || wait "$pid" 2>/dev/null
	done
	rm -rf "$queue_dir"
}
trap cleanup_queue EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

queue_case() {
	case_selected "$2" || return
	local command cost
	case " $queued_labels " in *" $2 "*) echo "ERROR: duplicate case label $2" >&2; exit 2 ;; esac
	queued_labels="$queued_labels $2"
	printf -v command '%q ' "$@"
	case_commands[case_count]=$command
	case_labels[case_count]=$2
	cost=$(case_cost "$@")
	printf '%s %s\n' "$cost" "$case_count" >> "$queue_dir/order"
	case_count=$((case_count + 1))
}

case_worker() {
	local index=$1 timing_start timing_elapsed
    [ "${REGRESS_TIMINGS:-0}" != 1 ] || timing_start=$(now)
	failures=0
	# shellcheck disable=SC2034 # consumed by the case helpers in the sourcing suite
	CASE_WORKER=1
	# Only strings made by printf %q above are evaluated, never game/log data.
	eval "${case_commands[$index]}"
    if [ "${REGRESS_TIMINGS:-0}" = 1 ]; then
        timing_elapsed=$(awk "BEGIN { printf \"%.2f\", $(now) - $timing_start }")
        echo "TIMING ${case_labels[$index]} ${timing_elapsed}s"
    fi
	printf '%s\n' "$failures" > "$queue_dir/$index.result"
}

run_queued_cases() {
	local next=0 finished=0 printed=0 slot index pid status case_failures
	local progressed order_index=0
	if [ "$case_count" -eq 0 ]; then echo "No regression cases selected."; return; fi
	if [ "$JOBS" -gt 1 ]; then
		# Stable tie-break by declaration index; Bash 3.2 has no wait -n.
		while read -r _ index; do
			case_order[order_index]=$index
			order_index=$((order_index + 1))
		done < <(LC_ALL=C sort -k1,1nr -k2,2n "$queue_dir/order")
	else
		for ((index=0; index<case_count; index++)); do case_order[index]=$index; done
	fi
	# There is no benefit in creating more worker slots than cases.
	[ "$JOBS" -le "$case_count" ] || JOBS=$case_count
	while [ "$finished" -lt "$case_count" ]; do
		progressed=0
		for ((slot=0; slot<JOBS; slot++)); do
			pid=${active_pids[$slot]:-}
			if [ -n "$pid" ]; then
				index=${active_cases[$slot]}
				if [ -f "$queue_dir/$index.result" ] || ! kill -0 "$pid" 2>/dev/null; then
					wait "$pid" 2>> "$queue_dir/$index.output"
					status=$?
					if [ "$status" -eq 0 ] && [ -f "$queue_dir/$index.result" ]; then
						read -r case_failures < "$queue_dir/$index.result"
					else
						echo "FAIL ${case_labels[$index]}: worker failed ($(exit_status_description "$status"))" >> "$queue_dir/$index.output"
						case_failures=1
					fi
					failures=$((failures + case_failures))
					: > "$queue_dir/$index.done"
					active_pids[slot]=''
					finished=$((finished + 1))
					progressed=1
				fi
			fi
			if [ -z "${active_pids[$slot]:-}" ] && [ "$next" -lt "$case_count" ]; then
				index=${case_order[$next]}
				case_worker "$index" > "$queue_dir/$index.output" 2>&1 &
				active_pids[slot]=$!
				active_cases[slot]=$index
				next=$((next + 1))
				progressed=1
			fi
		done
		# Emit whole case buffers only when all earlier declarations printed.
		while [ "${SORT_SUMMARY:-0}" -eq 0 ] && [ "$printed" -lt "$case_count" ] && [ -f "$queue_dir/$printed.done" ]; do
			cat "$queue_dir/$printed.output"
			printed=$((printed + 1))
		done
		[ "$progressed" -ne 0 ] || sleep 0.05
	done
    if [ "${SORT_SUMMARY:-0}" -eq 1 ]; then
        for ((index=0; index<case_count; index++)); do
            printf '%s %s\n' "${case_labels[$index]}" "$index"
        done | LC_ALL=C sort -k1,1 > "$queue_dir/summary-order"
        while read -r _ index; do cat "$queue_dir/$index.output"; done < "$queue_dir/summary-order"
    fi
}

