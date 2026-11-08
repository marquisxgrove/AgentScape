#!/bin/bash
# Applies the repository rulesets in .github/rulesets/*.json to GitHub, so branch protection lives in code.
#
#   tools/apply_rulesets.sh            # create or update every ruleset (needs repo admin)
#   tools/apply_rulesets.sh --check    # validate the files and show what would change; changes nothing
#
# Each file is a ruleset in the REST API's format. A ruleset is matched to the live one by its "name": it is
# updated in place if it exists and created otherwise. Rulesets on GitHub that have no file are left alone.
# Needs the GitHub CLI (`gh auth login` as a repository admin) and jq. Override the repository with
# AS_REPO=owner/name.
set -euo pipefail
cd "$(dirname "$0")/.."

CHECK=0
[[ "${1:-}" == "--check" ]] && CHECK=1

# owner/name from the origin remote (https://github.com/owner/name.git or git@github.com:owner/name.git).
REPO=${AS_REPO:-$(git remote get-url origin | sed -E 's#\.git$##; s#^.*[:/]([^/]+/[^/]+)$#\1#')}
CI=.github/workflows/ci.yml

shopt -s nullglob
files=(.github/rulesets/*.json)
[[ ${#files[@]} -gt 0 ]] || { echo "No rulesets in .github/rulesets/"; exit 1; }

live=$(gh api "repos/$REPO/rulesets" --jq '[.[] | {id, name}]')
status=0
for f in "${files[@]}"; do
  jq empty "$f" || { echo "$f: not valid JSON"; status=1; continue; }
  name=$(jq -r '.name' "$f")

  # A required check that no CI job reports would block every pull request, so refuse to apply one.
  while read -r ctx; do
    [[ -z "$ctx" ]] && continue
    if ! grep -Eq "(name: ${ctx}\$|name: ${ctx}[ ,}])" "$CI"; then
      echo "$f: required check '$ctx' is not a job name in $CI; it would never report and would block every PR"
      status=1
    fi
  done < <(jq -r '.rules[] | select(.type == "required_status_checks") | .parameters.required_status_checks[].context' "$f")
  [[ $status -eq 0 ]] || continue

  id=$(jq -r --arg n "$name" '.[] | select(.name == $n) | .id' <<<"$live")
  if [[ $CHECK -eq 1 ]]; then
    if [[ -z "$id" ]]; then
      echo "$name: not on GitHub yet; would be created"
    else
      # Compare only the fields the file controls.
      want=$(jq -S '{enforcement, conditions, bypass_actors, rules}' "$f")
      have=$(gh api "repos/$REPO/rulesets/$id" | jq -S '{enforcement, conditions, bypass_actors, rules}')
      if diff <(echo "$have") <(echo "$want") > /dev/null; then
        echo "$name: up to date (ruleset $id)"
      else
        echo "$name: differs from GitHub (ruleset $id); would be updated:"
        diff <(echo "$have") <(echo "$want") || true
      fi
    fi
  elif [[ -z "$id" ]]; then
    id=$(gh api -X POST "repos/$REPO/rulesets" --input "$f" --jq '.id')
    echo "$name: created ruleset $id"
  else
    gh api -X PUT "repos/$REPO/rulesets/$id" --input "$f" --jq '.id' > /dev/null
    echo "$name: updated ruleset $id"
  fi
done
exit $status
