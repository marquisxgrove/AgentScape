# The documented demo path as a headless smoke test (quest-logs task 9). Phase 1 plays the whole path in one process:
# guest -> Costumer (Adventurer + Kiroween gear) -> Codex card -> Quest Log -> +250 XP and level-up -> Hiscores.
# Phase 2 is a SEPARATE process on the same database (a reload, on the web) and checks it all survived.
# Invoked with -DAGENTSCAPE=<exe> -DOUTDIR=<dir>.
file(REMOVE_RECURSE "${OUTDIR}")
file(MAKE_DIRECTORY "${OUTDIR}")
set(DATADIR "${OUTDIR}/data")
file(MAKE_DIRECTORY "${DATADIR}")

foreach(phase 1 2)
  execute_process(
    COMMAND "${AGENTSCAPE}" "--data-dir=${DATADIR}" "--kiroween=on" "--smoke-path=${OUTDIR}" "--smoke-phase=${phase}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  message(STATUS "phase ${phase} output:\n${out}")
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "demo path phase ${phase} exited with ${rc}")
  endif()
  if(NOT out MATCHES "SMOKE OK: demo path phase ${phase}")
    message(FATAL_ERROR "demo path phase ${phase} did not report success")
  endif()
endforeach()

foreach(shot p1-login.png p2-world.png p3-costumer-create.png p4-costumer-equipped.png p5-codex.png p6-codex-card.png
             p7-quest-scroll.png p8-level-up.png p9-hiscores.png p10-restored-tavern.png)
  if(NOT EXISTS "${OUTDIR}/${shot}")
    message(FATAL_ERROR "demo path did not produce ${shot}")
  endif()
endforeach()
message(STATUS "demo path OK: 10 snapshots in ${OUTDIR}, and the state survived a restart")
