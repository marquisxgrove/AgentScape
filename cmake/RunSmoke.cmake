# Runs the headless demo-path smoke test and checks it produced the expected snapshots.
# Invoked by CTest with -DAGENTSCAPE=<exe> -DOUTDIR=<dir>.
file(REMOVE_RECURSE "${OUTDIR}")
file(MAKE_DIRECTORY "${OUTDIR}")

# Use a throwaway data dir so the test never touches the user's save.
set(DATADIR "${OUTDIR}/data")
file(MAKE_DIRECTORY "${DATADIR}")

execute_process(
  COMMAND "${AGENTSCAPE}" "--data-dir=${DATADIR}" "--kiroween=on" "--smoke-demo=${OUTDIR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message(STATUS "smoke output:\n${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "smoke demo exited with ${rc}")
endif()

foreach(shot 01-login.png 02-world.png 03-context-menu.png 04-entered.png)
  if(NOT EXISTS "${OUTDIR}/${shot}")
    message(FATAL_ERROR "smoke demo did not produce ${shot}")
  endif()
endforeach()
message(STATUS "smoke demo OK: 4 snapshots in ${OUTDIR}")
