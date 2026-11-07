# Costumer persistence smoke test (character-creator task 7): phase 1 creates an Adventurer, claims and
# equips the Jack-o'-Lantern Mask and Reaper's Scythe, and saves; phase 2 is a SEPARATE app process on the
# same database and checks the gear is still equipped. Invoked with -DAGENTSCAPE=<exe> -DOUTDIR=<dir>.
file(REMOVE_RECURSE "${OUTDIR}")
file(MAKE_DIRECTORY "${OUTDIR}")
set(DATADIR "${OUTDIR}/data")
file(MAKE_DIRECTORY "${DATADIR}")

foreach(phase 1 2)
  execute_process(
    COMMAND "${AGENTSCAPE}" "--data-dir=${DATADIR}" "--kiroween=on" "--smoke-costumer=${OUTDIR}" "--smoke-phase=${phase}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  message(STATUS "phase ${phase} output:\n${out}")
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "costumer smoke phase ${phase} exited with ${rc}")
  endif()
endforeach()

foreach(shot c1-create.png c2-equipped.png c3-restored.png)
  if(NOT EXISTS "${OUTDIR}/${shot}")
    message(FATAL_ERROR "costumer smoke did not produce ${shot}")
  endif()
endforeach()
message(STATUS "costumer smoke OK: gear survived an app restart")
