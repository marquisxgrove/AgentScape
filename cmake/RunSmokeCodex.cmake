# Codex flow smoke test (agent-codex task 9). Runs the app headlessly with Kiroween on and a throwaway data dir,
# and checks that every step passed and produced its snapshot. Invoked with -DAGENTSCAPE=<exe> -DOUTDIR=<dir>.
file(REMOVE_RECURSE "${OUTDIR}")
file(MAKE_DIRECTORY "${OUTDIR}")
set(DATADIR "${OUTDIR}/data")
file(MAKE_DIRECTORY "${DATADIR}")

execute_process(
  COMMAND "${AGENTSCAPE}" "--data-dir=${DATADIR}" "--kiroween=on" "--smoke-codex=${OUTDIR}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
message(STATUS "codex smoke output:\n${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "codex smoke exited with ${rc}")
endif()
if(NOT out MATCHES "SMOKE OK: codex flow completed")
  message(FATAL_ERROR "codex smoke did not report success")
endif()

foreach(shot x1-codex-search.png x2-card.png x3-card-debugging.png x4-proving.png x5-graveyard.png x6-level-up.png x7-tavern.png x8-quest-detail.png x9-hiscores.png)
  if(NOT EXISTS "${OUTDIR}/${shot}")
    message(FATAL_ERROR "codex smoke did not produce ${shot}")
  endif()
endforeach()
message(STATUS "codex smoke OK: 9 snapshots in ${OUTDIR}")
