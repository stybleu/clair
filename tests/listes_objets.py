class Point:
    def __init__(s, x, y): s.x, s.y = x, y
class Fixe:
    def __init__(s, a, b): s.a, s.b = a, b
class Noeud:
    def __init__(s, nom, enfants): s.nom, s.enfants = nom, enfants
def nuage(n): return [Point(float(i), float(i*i)) for i in range(n)]
def ajoute_point(ps, x): ps.append(Point(x, -x))
def arbre(prof, nom):
    n = Noeud(nom, [])
    if prof > 0:
        for i in range(2): n.enfants.append(arbre(prof-1, nom+str(i)))
    return n
def feuilles(n, acc):
    if len(n.enfants) == 0: acc.append(n.nom)
    for e in n.enfants: feuilles(e, acc)
def matrice(n):
    m = []
    for i in range(n):
        m.append([0]*n)
        for j in range(n): m[i][j] = i*n+j
    return m
def transpose(m): return [[m[i][j] for i in range(len(m))] for j in range(len(m[0]))]
def g(v):
    if isinstance(v, bool): return "1" if v else "0"
    if isinstance(v, float) and v == int(v): return str(int(v))
    return str(v)
out=[]
p=lambda v: out.append(g(v))
ps=nuage(6); p(len(ps)); p(ps[5].y); ajoute_point(ps,2.5); p(len(ps)); p(ps[-1].y)
morceau=ps[1:3]; p(len(morceau)); morceau[0].x=100.0; p(ps[1].x)
triple=[Point(0.0,0.0)]*3; triple[0].x=7.0; p(triple[2].x)
fs=[Fixe(i,i*2) for i in range(4)]; p(sum(f.a+f.b for f in fs)); p(fs[3].b)
racine=arbre(4,"r"); noms=[]; feuilles(racine,noms); p(len(noms)); p(noms[0]+" "+noms[15])
m=matrice(4); p(m[3][2]); t=transpose(m); p(t[2][3]); p(matrice(3)[2][1])
carres=[[i,i*i] for i in range(5)]; p(carres[4][1]); pts=[Point(float(i),0.0) for i in range(3)]; p(pts[2].x)
mots=[["m"+str(i)+str(j) for j in range(i+1)] for i in range(3)]; p(mots[2][1]); p(sum(len(l) for l in mots))
grille=[[0]*3 for i in range(2)]; grille[1][2]+=5; p(grille[1][2])
profond=[[[1,2],[3]],[[4]]]; profond[0][1].append(9); profond[0][1][1]=8; p(profond[0][1][1]); p(len(profond[0]))
cube=[[[a,b] for b in range(2)] for a in range(2)]; p(cube[1][0][0]+cube[1][1][1])
index={}
for i in range(6):
    k=i%3
    if not (k in index): index[k]=[]
    index[k].append(Point(float(i),0.0))
p(len(index[2])); p(index[2][1].x)
par_nom={"r":racine}; p(par_nom["r"].enfants[1].enfants[0].nom)
print("\n".join(out))
