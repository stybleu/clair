<?php
// une page PHP servie par le serveur Clair (pkg install php)
$nom = htmlspecialchars($_GET['nom'] ?? 'toi');
?>
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="/static/style.css">
<h1>Bonjour <?= $nom ?> depuis PHP <?= PHP_VERSION ?></h1>
<p><a href="/">Retour au carnet</a></p>
