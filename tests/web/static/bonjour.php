<?php
$nom = isset($_GET['nom']) ? $_GET['nom'] : 'personne';
echo "PHP dit bonjour a " . htmlspecialchars($nom) . "\n";
if ($_SERVER['REQUEST_METHOD'] === 'POST') { echo "poste : " . htmlspecialchars($_POST['x'] ?? '') . "\n"; }
