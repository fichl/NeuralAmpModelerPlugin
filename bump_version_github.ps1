$ErrorActionPreference = "Stop"

Set-Location -LiteralPath $PSScriptRoot

function Get-PythonCommand {
  $pyLauncher = Get-Command py -ErrorAction SilentlyContinue
  if ($pyLauncher) {
    return @($pyLauncher.Source, "-3")
  }

  $python = Get-Command python -ErrorAction SilentlyContinue
  if ($python) {
    return @($python.Source)
  }

  $python3 = Get-Command python3 -ErrorAction SilentlyContinue
  if ($python3) {
    return @($python3.Source)
  }

  throw "Python non trovato nel PATH."
}

$version = (Read-Host "Versione da impostare, senza v iniziale, es. 2.2.1").Trim()
if ($version -notmatch '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$') {
  throw "Versione non valida. Usa il formato major.minor.patch, ad esempio 2.2.1."
}

if ($version.StartsWith("v")) {
  throw "Scrivi solo i numeri, senza v iniziale."
}

$trackedStatus = git status --porcelain --untracked-files=no
if ($LASTEXITCODE -ne 0) {
  throw "Impossibile leggere lo stato Git."
}
if ($trackedStatus) {
  throw "Ci sono modifiche tracciate non committate. Commit/stash prima di fare il version bump."
}

$pythonCommand = Get-PythonCommand
$pythonArgs = @()
if ($pythonCommand.Count -gt 1) {
  $pythonArgs = $pythonCommand[1..($pythonCommand.Count - 1)]
}
& $pythonCommand[0] @pythonArgs ".\set_version.py" $version
if ($LASTEXITCODE -ne 0) {
  throw "set_version.py non è riuscito."
}

$changed = git status --porcelain --untracked-files=no
if (-not $changed) {
  Write-Host "Tutti i riferimenti di release sono già alla versione $version."
  Write-Host "La versione hardcoded della pagina Settings, 0.7.15, è rimasta invariata."
  exit 0
}

git -c core.autocrlf=false add `
  NeuralAmpModeler/config.h `
  NeuralAmpModeler/installer/NeuralAmpModeler.iss `
  NeuralAmpModeler/resources
if ($LASTEXITCODE -ne 0) {
  throw "git add non riuscito."
}

git -c core.autocrlf=false commit -m "Bump version to $version"
if ($LASTEXITCODE -ne 0) {
  throw "git commit non riuscito."
}

$branch = (git branch --show-current).Trim()
if (-not $branch) {
  throw "Branch corrente non trovato; non posso fare push."
}

git -c core.autocrlf=false push origin $branch
if ($LASTEXITCODE -ne 0) {
  throw "git push non riuscito."
}

Write-Host "Versione aggiornata a $version e pushata su origin/$branch."
Write-Host "La versione hardcoded della pagina Settings, 0.7.15, è rimasta invariata."
