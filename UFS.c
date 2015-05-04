#include "UFS.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disque.h"

// Quelques fonctions qui pourraient vous être utiles
int NumberofDirEntry(int Size) {
	return Size/sizeof(DirEntry);
}

int min(int a, int b) {
	return a<b ? a : b;
}

int max(int a, int b) {
	return a>b ? a : b;
}

/* Cette fonction va extraire le repertoire d'une chemin d'acces complet, et le copier
   dans pDir.  Par exemple, si le chemin fourni pPath="/doc/tmp/a.txt", cette fonction va
   copier dans pDir le string "/doc/tmp" . Si le chemin fourni est pPath="/a.txt", la fonction
   va retourner pDir="/". Si le string fourni est pPath="/", cette fonction va retourner pDir="/".
   Cette fonction est calquée sur dirname(), que je ne conseille pas d'utiliser car elle fait appel
   à des variables statiques/modifie le string entrant. Voir plus bas pour un exemple d'utilisation. */
int GetDirFromPath(const char *pPath, char *pDir) {
  strcpy(pDir,pPath);
  int len = strlen(pDir); // length, EXCLUDING null
  int index;
	
  // On va a reculons, de la fin au debut
  while (pDir[len]!='/') {
    len--;
    if (len <0) {
      // Il n'y avait pas de slash dans le pathname
      return 0;
    }
  }
  if (len==0) { 
    // Le fichier se trouve dans le root!
    pDir[0] = '/';
    pDir[1] = 0;
  }
  else {
    // On remplace le slash par une fin de chaine de caractere
    pDir[len] = '\0';
  }
  return 1;
}

/* Cette fonction va extraire le nom de fichier d'une chemin d'acces complet.
   Par exemple, si le chemin fourni pPath="/doc/tmp/a.txt", cette fonction va
   copier dans pFilename le string "a.txt" . La fonction retourne 1 si elle
   a trouvée le nom de fichier avec succes, et 0 autrement. Voir plus bas pour
   un exemple d'utilisation. */   
int GetFilenameFromPath(const char *pPath, char *pFilename) {
  // Pour extraire le nom de fichier d'un path complet
  char *pStrippedFilename = strrchr(pPath,'/');
  if (pStrippedFilename!=NULL) {
    ++pStrippedFilename; // On avance pour passer le slash
    if ((*pStrippedFilename) != '\0') {
      // On copie le nom de fichier trouve
      strcpy(pFilename, pStrippedFilename);
      return 1;
    }
  }
  return 0;
}

/* Un exemple d'utilisation des deux fonctions ci-dessus :
int bd_create(const char *pFilename) {
	char StringDir[256];
	char StringFilename[256];
	if (GetDirFromPath(pFilename, StringDir)==0) return 0;
	GetFilenameFromPath(pFilename, StringFilename);
	                  ...
*/


/* Cette fonction sert à afficher à l'écran le contenu d'une structure d'i-node */
void printiNode(iNodeEntry iNode) {
  printf("\t\t========= inode %d ===========\n",iNode.iNodeStat.st_ino);
  printf("\t\t  blocks:%d\n",iNode.iNodeStat.st_blocks);
  printf("\t\t  size:%d\n",iNode.iNodeStat.st_size);
  printf("\t\t  mode:0x%x\n",iNode.iNodeStat.st_mode);
  int index = 0;
  for (index =0; index < N_BLOCK_PER_INODE; index++) {
    printf("\t\t      Block[%d]=%d\n",index,iNode.Block[index]);
  }
}


/* ----------------------------------------------------------------------------------------
					            à vous de jouer, maintenant!
   ---------------------------------------------------------------------------------------- */
					 

/*
** Retourne le nombre de bloc libre
*/
int	bd_countfreeblocks(void) {
  char	buffer[BLOCK_SIZE];
  int	count = 0;

  if (ReadBlock(FREE_BLOCK_BITMAP, buffer) > 0) {
    int i;
    for (i = 0 ; i < BLOCK_SIZE ; ++i) {
      if (buffer[i] != 0)
	count++;
    }
  }
  return count;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 Si le fichier n'existe pas
*/
int	bd_stat(const char *pFilename, gstat *pStat) {
  iNodeEntry	*fileNode;

  fileNode = getInodeFromFile(pFilename);
  if (fileNode == NULL)	// gestion d'erreur
    return -1;

  pStat->st_ino = fileNode->iNodeStat.st_ino;
  pStat->st_mode = fileNode->iNodeStat.st_mode;
  pStat->st_nlink = fileNode->iNodeStat.st_nlink;
  pStat->st_size = fileNode->iNodeStat.st_size;
  pStat->st_blocks = fileNode->iNodeStat.st_blocks;
  return 0;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 si le dossier n'existe pas
** Retourne -2 si le fichier existe déjà
*/
int	bd_create(const char *pFilename) {
  char		directory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeNewFile;
  char		file[256];

  // gestion d'erreur
  if (GetDirFromPath(pFilename, directory) != 1)
    return -1;
  if ((nodeDirectory = checkDirExist(directory)) == NULL) {
    return -1;
  }
  if ((nodeNewFile = checkFileExist(pFilename, nodeDirectory)) != NULL)
    return -2;

  nodeNewFile = getNewNode();
  if (nodeNewFile != NULL && GetFilenameFromPath(pFilename, file) == 1) {
    addFilenameToDirectory(nodeDirectory, file, nodeNewFile->iNodeStat.st_ino);
    nodeNewFile->iNodeStat.st_mode = G_IFREG | G_IRWXU | G_IRWXG;
    nodeNewFile->iNodeStat.st_nlink = 1;
    nodeNewFile->iNodeStat.st_size = 0;
    nodeNewFile->iNodeStat.st_blocks = 0;
    SaveInode(nodeNewFile);
  } else {
    return -2;
  }

  return 0;
}

/*
** Retourne 0 si offset > taille du fichier
** Retourne -1 si le fichier n'existe pas
** Retourne -2 si le répertoire n'existe pas
** Sinon, retourne le nombre d'octet lu
*/
int	bd_read(const char *pFilename, char *buffer, int offset, int numbytes) {
  char	directory[256];
  int	count = 0;
  int	firstBlockToRead;
  int	offsetInBlockToRead;
  int	offsetBuffer = 0;
  char	readBuffer[BLOCK_SIZE];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeFile;

  // gestion d'erreur
  if (GetDirFromPath(pFilename, directory) != 1)
    return -1;
  
  if ((nodeDirectory = checkDirExist(directory)) == NULL) {
    printf("Le dossier parent n'existe pas\n");
    return -1;
  }
  
  if ((nodeFile = checkFileExist(pFilename, nodeDirectory)) == NULL)  {
    printf("Le fichier %s est inexistant!\n", pFilename);
    return -1;
  }

  if (nodeFile != NULL && isDir(nodeFile))  {
    printf("Le fichier %s est un répertoire!\n", pFilename);
    return -2;
  }
  if (offset > nodeFile->iNodeStat.st_size)
    return 0;

  // On récupère le premier bloc et le premier octet du bloc à lire
  firstBlockToRead = offset / BLOCK_SIZE;
  offsetInBlockToRead = offset % BLOCK_SIZE;

  // Tant que je n'ai pas lu le nombre de caractères demandés
  // OU que je n'ai pas atteint la limite de bloc du fichier
  // OU que je n'ai pas atteint la taille limite du fichier
  while (numbytes - count > 0 && firstBlockToRead < nodeFile->iNodeStat.st_blocks && (offset + count) < nodeFile->iNodeStat.st_size) {
    if (ReadBlock(nodeFile->Block[firstBlockToRead], readBuffer) > 0) {
      // Tant que je n'ai pas fini de lire le bloc
      // OU que mon nombre de caractères à lire n'ai pas atteint
      while (offsetInBlockToRead < BLOCK_SIZE && numbytes - count > 0 && (offset + count) < nodeFile->iNodeStat.st_size) {
	buffer[offsetBuffer] = readBuffer[offsetInBlockToRead];
	offsetInBlockToRead++;
	offsetBuffer++;
	count++;
      }
      offsetInBlockToRead = 0;
      firstBlockToRead++;
    } else {
      break;
    }
  }

  return count;
}

/*
** Retourne -1 si le fichier n'existe pas
** Retourne -2 si le fichier est un répertoire
** Retourne -3 si l'offset est trop grand
** Retourne -4 si l'offset est plus grand que la taille maximal d'un fichier
** Sinon retourne la nombre de bytes écrit dans le fichier
*/
int		bd_write(const char *pFilename, const char *buffer, int offset, int numbytes) {
  char		directory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeFile;
  char		file[256];
  int		firstBlockToRead = 0;
  int		offsetInBlockToRead = 0;
  int		offsetBuffer = 0;
  int		count = 0;
  char		writeBuffer[256];

  // gestion d'erreur
  if (GetDirFromPath(pFilename, directory) != 1)
    return -1;
  if ((nodeDirectory = checkDirExist(directory)) == NULL) {
    return -1;
  }
  if ((nodeFile = checkFileExist(pFilename, nodeDirectory)) == NULL)
    return -1;

  if (!isReg(nodeFile))
    return -2;

  if (offset > nodeFile->iNodeStat.st_size) {
    printf("L'offset demande est trop grand pour %s!\n", pFilename);
    return -3;
  }
  if (offset > DISKSIZE)
    return -4;

  if (GetFilenameFromPath(pFilename, file) != 1)
    return -1;

  // On récupère le premier bloc et le premier octet du bloc à lire
  firstBlockToRead = offset / BLOCK_SIZE;
  offsetInBlockToRead = offset % BLOCK_SIZE;

  // Tant que je n'ai pas lu le nombre de caractères demandés
  // OU que je n'ai pas atteint la limite de bloc du fichier
  // OU que je n'ai pas atteint la taille limite du fichier
  while (numbytes - count > 0 && firstBlockToRead < N_BLOCK_PER_INODE) {
    if (nodeFile->iNodeStat.st_blocks <= firstBlockToRead)
      nodeFile->Block[firstBlockToRead] = getNewBlock();


    if (ReadBlock(nodeFile->Block[firstBlockToRead], writeBuffer) > 0) {
      // Tant que je n'ai pas fini de lire le bloc
      // OU que mon nombre de caractères à lire n'ai pas atteint
      while (offsetInBlockToRead < BLOCK_SIZE && numbytes - count > 0) {
	writeBuffer[offsetInBlockToRead] = buffer[offsetBuffer];
	offsetInBlockToRead++;
	offsetBuffer++;
	count++;
      }
      if (WriteBlock(nodeFile->Block[firstBlockToRead], writeBuffer) <= 0)
	break;
      if (numbytes - count > 0) {
	offsetInBlockToRead = 0;
	firstBlockToRead++;
      }
    } else {
      break;
    }
  }

  if (firstBlockToRead == N_BLOCK_PER_INODE)
    printf("Le fichier pFilename deviendra trop gros!\n");


  nodeFile->iNodeStat.st_size = (offset + numbytes < nodeFile->iNodeStat.st_size) ? nodeFile->iNodeStat.st_size : (offset + count);
  nodeFile->iNodeStat.st_blocks = (firstBlockToRead >= nodeFile->iNodeStat.st_blocks) ? firstBlockToRead + 1 : nodeFile->iNodeStat.st_blocks;
  SaveInode(nodeFile);

  return count;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 si le chemin est inexistant ou autre erreur (probleme d'écriture, etc à défaut d'avoir un code pour cela)
** Retourne -2 si le fichier existe déjà
*/
int		bd_mkdir(const char *pDirName) {
  char		directory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeNewFile;
  char		file[256];

  // gestion d'erreur
  if (GetDirFromPath(pDirName, directory) != 1)
    return -1;
  if ((nodeDirectory = checkDirExist(directory)) == NULL) {
    return -1;
  }
  if ((nodeNewFile = checkFileExist(pDirName, nodeDirectory)) != NULL)
    return -2;

  nodeNewFile = getNewNode();
  if (nodeNewFile != NULL && GetFilenameFromPath(pDirName, file) == 1) {
    if (addFilenameToDirectory(nodeDirectory, file, nodeNewFile->iNodeStat.st_ino) == -1 || initDirectory(nodeNewFile, nodeDirectory) == -1)
      return -1;
    SaveInode(nodeDirectory);
    SaveInode(nodeNewFile);
  } else {
    return -2;
  }

  return 0;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 si le répertoire dans pPathNouveauLien n'existe pas
** Retourne -1 si pPathExistant n'existe pas
** Retourne -2 si le nouveau lien existe déjà
** Retourne -3 si le fichier est un répertoire
*/	
int bd_hardlink(const char *pPathExistant, const char *pPathNouveauLien) {
  char		directory[256];
  char		newdirectory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeNewDirectory;
  iNodeEntry	*nodeFile;
  iNodeEntry	*nodeNewFile;
  char		file[256];

  // gestion d'erreur
  if (GetDirFromPath(pPathExistant, directory) != 1 ||
      GetDirFromPath(pPathNouveauLien, newdirectory) != 1)
    return -1;

  if ((nodeDirectory = checkDirExist(directory)) == NULL ||
      (nodeNewDirectory = checkDirExist(newdirectory)) == NULL)
    return -1;

  if ((nodeFile = checkFileExist(pPathExistant, nodeDirectory)) == NULL)
    return -1;
  if ((nodeNewFile = checkFileExist(pPathNouveauLien, nodeNewDirectory)) != NULL)
    return -2;
  
  if (GetFilenameFromPath(pPathNouveauLien, file) == 1) {
    addFilenameToDirectory(nodeNewDirectory, file, nodeFile->iNodeStat.st_ino);
    nodeFile->iNodeStat.st_nlink++;
    SaveInode(nodeFile);
  } else {
    return -2;
  }
  
  return 0;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 si le fichier n'existe pas
** Retourne -2 si ce n'est pas un fichier régulier
*/
int bd_unlink(const char *pFilename) {
  char		directory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeFile;
  char		file[256];

  // gestion d'erreur
  if (GetDirFromPath(pFilename, directory) != 1)
    return -1;
  if ((nodeDirectory = checkDirExist(directory)) == NULL) {
    return -1;
  }
  if ((nodeFile = checkFileExist(pFilename, nodeDirectory)) == NULL)
    return -1;

  if (!isReg(nodeFile))
    return -2;

  if (GetFilenameFromPath(pFilename, file) != 1)
    return -1;

  int node = removeFilenameFromDirectory(nodeDirectory, file);
  if (node >= 0)  {
    nodeFile->iNodeStat.st_nlink--;
    SaveInode(nodeFile);
    if (nodeFile->iNodeStat.st_nlink == 0) {
      int i;
      for (i = 0 ; i < nodeFile->iNodeStat.st_blocks; ++i) {
	ReleaseFreeBlock(nodeFile->Block[i]);
      }
      ReleaseNode(nodeFile);
    }
  } else {
    return -1;
  }
  return 0;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 si le fichier est inexistant
** Retourne -2 si c'est un fichier régulier
** Retourne -3 si le dossier n'est pas vide (ou si c'est /)
*/
int bd_rmdir(const char *pFilename) {
  char		directory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeFile;
  char		file[256];

  // gestion d'erreur
  if (GetDirFromPath(pFilename, directory) != 1)
    return -1;
  if ((nodeDirectory = checkDirExist(directory)) == NULL) {
    return -1;
  }
  if ((nodeFile = checkFileExist(pFilename, nodeDirectory)) == NULL)
    return -1;

  if (!isDir(nodeFile))
    return -2;

  if (GetFilenameFromPath(pFilename, file) != 1)
    return -1;

  if (NumberofDirEntry(nodeFile->iNodeStat.st_size) > 2 || nodeFile->iNodeStat.st_ino == ROOT_INODE) {
    return -3;
  }

  int node = removeFilenameFromDirectory(nodeDirectory, file);
  if (node >= 0)  {
    nodeFile->iNodeStat.st_nlink -= 2;
    SaveInode(nodeFile);
    nodeDirectory->iNodeStat.st_nlink--;
    SaveInode(nodeDirectory);
    if (nodeFile->iNodeStat.st_nlink == 0) {
      ReleaseFreeBlock(nodeFile->Block[0]);
      ReleaseNode(nodeFile);
    }
  } else {
    return -1;
  }
  return 0;
}

/*
** Retourne 0 en cas de succès
** Retourne -1 si le fichier est inexistant ou si le répertoire pDestFilename n'existe pas
*/
int	bd_rename(const char *pFilename, const char *pDestFilename) {
  char		directory[256];
  char		newdirectory[256];
  iNodeEntry	*nodeDirectory;
  iNodeEntry	*nodeNewDirectory;
  iNodeEntry	*nodeFile;
  iNodeEntry	*nodeNewFile;
  char		file[256];

  // gestion d'erreur
  if (GetDirFromPath(pFilename, directory) != 1 ||
      GetDirFromPath(pDestFilename, newdirectory) != 1)
    return -1;

  if ((nodeDirectory = checkDirExist(directory)) == NULL ||
      (nodeNewDirectory = checkDirExist(newdirectory)) == NULL)
    return -1;

  if (strcmp(directory, newdirectory) == 0)
    nodeNewDirectory = nodeDirectory;

  if ((nodeFile = checkFileExist(pFilename, nodeDirectory)) == NULL)
    return -1;

  if ((nodeNewFile = checkFileExist(pDestFilename, nodeNewDirectory)) != NULL) {
    if (isDir(nodeFile)) {
      return -1;
    } else {
      if (bd_hardlink(pFilename, pDestFilename) != 0)
	return -1;
      if (bd_unlink(pDestFilename) != 0) {
	return -1;
      }
    }
  }

  if (GetFilenameFromPath(pFilename, file) == 1) {
    removeFilenameFromDirectory(nodeDirectory, file);    
  } else {
    return -1;
  }
  
  if (GetFilenameFromPath(pDestFilename, file) == 1) {
    addFilenameToDirectory(nodeNewDirectory, file, nodeFile->iNodeStat.st_ino);
    if (isDir(nodeFile)) {
      nodeDirectory->iNodeStat.st_nlink--;
      SaveInode(nodeDirectory);
      nodeNewDirectory->iNodeStat.st_nlink++;
      SaveInode(nodeNewDirectory);
      updateParent(nodeFile, nodeNewDirectory);
    }
  } else {
    return -1;
  }
  
  return 0;
}

/*
** Retourne le nombre d'entrée en cas de succès
** Retourne -1 en cas d'erreur
*/
int		bd_readdir(const char *pDirLocation, DirEntry **ppListeFichiers) {
  iNodeEntry	*nodeDirectory;
  char		buffer[BLOCK_SIZE];
  DirEntry	*originalList;
  DirEntry	*listFiles;

  if ((nodeDirectory = checkDirExist(pDirLocation)) == NULL) {
    return -1;
  }

  if ((listFiles = malloc(NumberofDirEntry(nodeDirectory->iNodeStat.st_size) * sizeof(DirEntry))) == NULL)
    return -1;

  if (ReadBlock(nodeDirectory->Block[0], buffer) > 0) {
    originalList = (DirEntry *)buffer;
    int i;
    for (i = 0 ; i < NumberofDirEntry(nodeDirectory->iNodeStat.st_size) ; ++i) {
      listFiles[i] = (DirEntry)originalList[i];
    }
  } else {
    free(listFiles);
    return -1;
  }
  
  *ppListeFichiers = listFiles;
  return NumberofDirEntry(nodeDirectory->iNodeStat.st_size);
}


/*
** Fonction utilitaire
*/

iNodeEntry	*getInodeFromFile(const char *path) {
  char		directory[256];
  iNodeEntry	*returnValue = NULL;

  // Récupère le nom du dossier
  if (GetDirFromPath(path, directory) != 1)
    return NULL;

  // Récupère la node du dossier
  iNodeEntry	*parentFolder = checkDirExist(directory);
  if (parentFolder != NULL) {
    // Récupère la node du fichier
    returnValue = checkFileExist(path, parentFolder);
  }
  return returnValue;
}

iNodeEntry	*checkFileExist(const char *path, iNodeEntry *directoryNode) {
  char		buffer[BLOCK_SIZE];

  if (directoryNode == NULL)
    return NULL;

  // Lis l'unique bloc du dossier
  if (ReadBlock(directoryNode->Block[0], buffer) > 0) {
    DirEntry	*directoryEntry = (DirEntry *)buffer;
    char	filename[256];

    if (strcmp(path, "/") != 0) {
      // Récupère le nom de fichier
      if (GetFilenameFromPath(path, filename) != 1)
	return NULL;
    } else {
      strcpy(filename, ".");
    }

    // Cherche le fichier dans le répertoire
    int i;
    for (i = 0 ; i < NumberofDirEntry(directoryNode->iNodeStat.st_size) ; ++i) {
      if (strcmp(directoryEntry[i].Filename, filename) == 0) {
	// Retourne son inode si trouvé
	return getInodeFromIno(directoryEntry[i].iNode);
      }
    }
  }
  return NULL;
}

iNodeEntry	*checkDirExist(const char *path) {
  iNodeEntry	*inode = NULL;

  if (path == NULL) {
    // Pour eviter les erreurs
    return NULL;
  } else if (strcmp(path, "/") == 0) {
    // On connait la node racine
    return rootINode();
  } else {
    // On a besoin du parent donc on l'initialise
    char	parent[256];

    if (GetDirFromPath(path, parent) != 1)
      return NULL;
    
    if (strcmp(parent, "/") == 0) {
      // Si le répertoire parent est la racine, on peut directement verifier et retourner la bonne node
      inode = checkFileExist(path, rootINode());
      // Est ce que notre fichier existe et est bien un dossier ?
      if (inode != NULL && isDir(inode) == 1) {
	return inode;
      } else {
	return NULL;
      }
    } else {
      // Recursivité sur le nouveau path
      iNodeEntry *inodeParent = checkDirExist(parent);
      if (inodeParent != NULL) {
	inode = checkFileExist(path, inodeParent);
      }
      return inode;
    }
  }
}

// Verifie que la node est un dossier
int	isDir(iNodeEntry *node) {
  if (node != NULL && node->iNodeStat.st_mode & G_IFDIR)
    return 1;
  return 0;
}

// Verifie que la node est un fichier
int	isReg(iNodeEntry *node) {
  if (node != NULL && node->iNodeStat.st_mode & G_IFREG)
    return 1;
  return 0;
}

// Retourne la node racine
iNodeEntry	*rootINode() {
  return getInodeFromIno(ROOT_INODE);
}

// Retourne une inode via son numéro
iNodeEntry	*getInodeFromIno(ino numNode) {
  char		*buffer;

  if (numNode > N_INODE_ON_DISK && numNode > 0)
    return NULL;

  if ((buffer = malloc(BLOCK_SIZE * sizeof(char))) == NULL)
    return NULL;

  if (ReadBlock(BASE_BLOCK_INODE + (numNode / NUM_INODE_PER_BLOCK), buffer) > 0) {
    return (iNodeEntry *)(buffer + (numNode % NUM_INODE_PER_BLOCK) * sizeof(iNodeEntry));
  }
  return NULL;
}

iNodeEntry	*getNewNode() {
  iNodeEntry	*newNode;
  char		buffer[BLOCK_SIZE];

  if (ReadBlock(FREE_INODE_BITMAP, buffer) > 0) {
    int i;
    for (i = ROOT_INODE ; i < N_INODE_ON_DISK ; ++i) {
      if (buffer[i] != 0) {
	newNode = getInodeFromIno(i);
	buffer[i] = 0;
	if (WriteBlock(FREE_INODE_BITMAP, buffer) > 0) {
	  printf("GLOFS: Saisie i-node %d\n", i);
	  return newNode;
	}
      }
    }
  }
  return NULL;
}

int		getNewBlock() {
  char		buffer[BLOCK_SIZE];

  if (ReadBlock(FREE_BLOCK_BITMAP, buffer) > 0) {
    int i;
    for (i = 0 ; i < N_BLOCK_ON_DISK ; ++i) {
      if (buffer[i] != 0) {
	buffer[i] = 0;
	if (WriteBlock(FREE_BLOCK_BITMAP, buffer) > 0) {
	  printf("GLOFS: Saisie bloc %d\n", i);
	  return i;
	}
      }
    }
  }
  return -1;
}

int	addFilenameToDirectory(iNodeEntry *nodeDirectory, char filename[256], ino nodeNumber) {
  char	bufferDirectory[BLOCK_SIZE];

  if (nodeDirectory == NULL || strlen(filename) >= FILENAME_SIZE)
    return -1;

  if (nodeDirectory->iNodeStat.st_size + sizeof(DirEntry) > BLOCK_SIZE)
    return -1;

  // Lis l'unique bloc du dossier
  if (ReadBlock(nodeDirectory->Block[0], bufferDirectory) > 0) {
    DirEntry	*directoryEntry = (DirEntry *)bufferDirectory;

    int i;
    for (i = 0 ; i < NumberofDirEntry(nodeDirectory->iNodeStat.st_size) ; ++i);
    strcpy(directoryEntry[i].Filename, filename);
    directoryEntry[i].iNode = nodeNumber;
    nodeDirectory->iNodeStat.st_size += sizeof(DirEntry);
    WriteBlock(nodeDirectory->Block[0], bufferDirectory);
    SaveInode(nodeDirectory);
  }
  return 0;
}

int	removeFilenameFromDirectory(iNodeEntry *nodeDirectory, char filename[256]) {
  char	bufferDirectory[BLOCK_SIZE];
  int	iNode = -1;

  if (nodeDirectory == NULL || strlen(filename) >= FILENAME_SIZE)
    return -1;

  if (NumberofDirEntry(nodeDirectory->iNodeStat.st_size) == 0 ||
      strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
    return -1;
 
  // Lis l'unique bloc du dossier
  if (ReadBlock(nodeDirectory->Block[0], bufferDirectory) > 0) {
    DirEntry	*directoryEntry = (DirEntry *)bufferDirectory;

    int i = 0;
    int j = 0;
    while (i < NumberofDirEntry(nodeDirectory->iNodeStat.st_size) &&
	   j < NumberofDirEntry(nodeDirectory->iNodeStat.st_size)) {
      if (strcmp(directoryEntry[i].Filename, filename) == 0)  {
	iNode = directoryEntry[i].iNode;
	j++;
      }
      if (i != j)  {
	directoryEntry[i].iNode = directoryEntry[j].iNode;
	strcpy(directoryEntry[i].Filename, directoryEntry[j].Filename);
      }
      i++;
      j++;
    }

    nodeDirectory->iNodeStat.st_size -= sizeof(DirEntry);

    if (WriteBlock(nodeDirectory->Block[0], bufferDirectory) <= 0 || SaveInode(nodeDirectory) == 0)
      return -1;
  }
  return iNode;
}

int	ReleaseFreeBlock(UINT16 BlockNum) {
  char	BlockFreeBitmap[BLOCK_SIZE];

  if (ReadBlock(FREE_BLOCK_BITMAP, BlockFreeBitmap) > 0) {
    BlockFreeBitmap[BlockNum] = 1;
    printf("GLOFS: Relache bloc %d\n",BlockNum);
    if (WriteBlock(FREE_BLOCK_BITMAP, BlockFreeBitmap) > 0)
      return 1;
  }
  return 0;
}

int	SaveInode(iNodeEntry *node) {
  char	block[BLOCK_SIZE];

  if (ReadBlock(BASE_BLOCK_INODE + (node->iNodeStat.st_ino / NUM_INODE_PER_BLOCK), block) > 0) {
    iNodeEntry	*whereToSave = (iNodeEntry *)(block + (node->iNodeStat.st_ino % NUM_INODE_PER_BLOCK) * sizeof(iNodeEntry));
    *whereToSave = *node;
    if (WriteBlock(BASE_BLOCK_INODE + (node->iNodeStat.st_ino / NUM_INODE_PER_BLOCK), block) > 0) {
      return 1;
    }
  }
  return 0;
}

int	initDirectory(iNodeEntry *nodeNewFile, iNodeEntry *nodeDirectory)  {
  int nbBlock = getNewBlock();
  if (nbBlock != -1) {
      nodeDirectory->iNodeStat.st_nlink++;

      nodeNewFile->iNodeStat.st_mode = G_IFDIR | G_IRWXU | G_IRWXG;
      nodeNewFile->iNodeStat.st_nlink = 2;
      nodeNewFile->iNodeStat.st_size = 0;
      nodeNewFile->iNodeStat.st_blocks = 1;
      nodeNewFile->Block[0] = nbBlock;
      addFilenameToDirectory(nodeNewFile, ".", nodeNewFile->iNodeStat.st_ino);
      addFilenameToDirectory(nodeNewFile, "..", nodeDirectory->iNodeStat.st_ino);
      return 0;
  }
  return -1;
}

int		ReleaseNode(iNodeEntry *node) {
  char		buffer[BLOCK_SIZE];

  if (ReadBlock(FREE_INODE_BITMAP, buffer) > 0) {
    buffer[node->iNodeStat.st_ino] = 1;
    if (WriteBlock(FREE_INODE_BITMAP, buffer) > 0) {
      printf("GLOFS: Relache i-node %d\n", node->iNodeStat.st_ino);
      return node->iNodeStat.st_ino;
    }
  }
  return 0;
}

void		updateParent(iNodeEntry *nodeNewFile, iNodeEntry* nodeNewDirectory) {
  char	bufferDirectory[BLOCK_SIZE];

  if (nodeNewDirectory == NULL || nodeNewFile == NULL)
    return;

  // Lis l'unique bloc du dossier
  if (ReadBlock(nodeNewFile->Block[0], bufferDirectory) > 0) {
    DirEntry	*directoryEntry = (DirEntry *)bufferDirectory;

    int i = 0;
    for (i = 0 ; i < NumberofDirEntry(nodeNewFile->iNodeStat.st_size) ; ++i)  {
      if (strcmp(directoryEntry[i].Filename, "..") == 0)  {
	directoryEntry[i].iNode = nodeNewDirectory->iNodeStat.st_ino;
	break;
      }
    }
  }

  WriteBlock(nodeNewFile->Block[0], bufferDirectory);
}
