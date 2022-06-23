# include <iostream>
# include <stdio.h>
# include <queue>
# include <memory>
# include <utility>
# include <assert.h>

//红黑树：希望能够作为定时器的底层结构（typename只支持基础数据类型或本身支持大小比较的类型）
//特点：
//	（1）根节点为黑色；
//	（2）节点非红即黑；
//	（3）叶子节点（虚拟节点NIL）为黑色；
//	（4）红色节点的儿子节点必须为黑色（不能出现父子同红的情况）；
//	（5）根节点到每个叶子节点的路径上，其黑色节点数目相同；
//注意事项：
//	（1）默认要让红黑树所有叶子节点为NIL节点（黑色节点），这是为了方便后面的删除操作
//	（2）NIL节点为内部成员，绝对不能返回给用户，public中return需要注意这一点
//	（3）插入的所有节点都为红色节点，且需要调整的必定是父子同为红色的冲突；
//	（4）删除的节点为黑色时才需要进行调整；
template<typename T>
struct TreeNode {
	T key;
	void *value;
	int color;			//0为红色，1为黑色，2为双重黑
	TreeNode<T>* left;
	TreeNode<T>* right;
	TreeNode<T>* parent;
	TreeNode(const T &k, void *v = nullptr, int c = 0, TreeNode<T>* l = nullptr, TreeNode<T>* r = nullptr, TreeNode<T>* p = nullptr) :
		key(k), value(v), left(l), right(r), parent(p), color(c) {}
	TreeNode(const TreeNode &cp) :key(cp.key), value(cp.value), color(cp.color), left(cp.left), right(cp.right), parent(cp.parent) {
		// std::cout << "copy_constructor" << std::endl;
	}
	//TreeNode(TreeNode &&cp) {
	//	std::cout << "move_constructor" << std::endl;
	//	key = move(cp.key);
	//	value = move(cp.value);
	//	color = move(cp.color);
	//	left = cp.left;
	//	right = cp.right;
	//}
};

template<typename T>
class rbtree {
private:
	TreeNode<T>* root;	// 根节点
	TreeNode<T>* NIL;	// 虚拟空节点，方便红黑树节点颜色维护
	int treeSize;		// 树的节点数目

	TreeNode<T>* getNewNode(const T& key, void *value = nullptr);										// 获取新节点
	void swapNode(TreeNode<T> *&a, TreeNode<T> *&b);														// 交换两个节点（要把父亲和左右孩子节点的关系全部交接完成）
	TreeNode<T>* insertHelper(const T& key, void *value, TreeNode<T> *cur, TreeNode<T> *&newNode);		// 插入节点的起始点默认为root，需要分离
	TreeNode<T>* eraseHelper(T& key, TreeNode<T> *cur);													// 删除节点的起始点默认为root，需要分离
	TreeNode<T>* getHelper(const T& key, TreeNode<T> *cur);												// 查找节点的起始点默认为root，需要分离
	void printNodesHelper(TreeNode<T> *cur);															// 递归打印所有节点的详细信息（前序遍历）
	void clearHelper(TreeNode<T> *cur);																	// 清空树的起始点默认为root，需要分离
	TreeNode<T>* leftRotate(TreeNode<T> *cur);															// 对当前节点进行左旋
	TreeNode<T>* rightRotate(TreeNode<T> *cur);															// 对当前节点进行右旋
	bool hasRedChild(TreeNode<T> *cur);																	// 判断是否有红色儿子节点
	TreeNode<T>* insertMaintain(TreeNode<T> *cur);														// 在插入节点时维护红黑树路径下节点的颜色
	TreeNode<T>* deleteMaintain(TreeNode<T> *cur);														// 在删除节点时维护红黑树路径下节点的颜色
public:
	rbtree() {
		T tmp;
		NIL = new TreeNode<T>(tmp, nullptr);
		NIL->color = 1;
		NIL->left = NIL->right = NIL->parent = NIL; // 这里一定要后赋值，因为NIL这时才初始化完成
		root = NIL;
	}
	~rbtree() {
		clear();
	}

	bool insert(const T& key, void *value, TreeNode<T> *&newNode);			// 插入节点
	bool erase(T& key);														// 删除节点

	TreeNode<T>* get(const T& key);											// 根据key值获取节点
	TreeNode<T>* getMin();													// 获取最小节点
	TreeNode<T>* getMax();													// 获取最大节点

	TreeNode<T>* pop(const T& key);											// 根据key值获取节点，并从树中删除该节点
	TreeNode<T>* popMin();													// 获取并删除最小节点
	TreeNode<T>* popMax();													// 获取并删除最大节点

	int size();																// 打印红黑树当前有多少个节点
	void printTree();														// 层序打印整棵树
	void printAllNodes();													// 打印每个节点的详细信息
	void clear();															// 清空红黑树
};

/*=============================================== private 函数 ===============================================*/

template<typename T>
TreeNode<T>* rbtree<T>::getNewNode(const T& key, void *value) {
	return new TreeNode<T>(key, value, 0, NIL, NIL, NIL);
}

template<typename T>
void rbtree<T>::swapNode(TreeNode<T> *&a, TreeNode<T> *&b) {
	// 1、这里交换节点不能直接使用swap，因为NIL节点的parent是混乱的，无法确保正确，所以在swap之前要进行条件判断
	// 2、要考虑特殊情况，因为有可能出现交换的两个节点为相邻节点
	//	  而相邻节点最大的一个问题就是存在同样的边被交换了两次，这里我理不清关系
	//	  只能暴力枚举情况了

	// 节点交换时颜色不能换
	std::swap(a->color, b->color);
	// 判断两者是否为相邻节点
	int flag = 0;//0表示不相邻，1表示a的左孩子为b，2表示a的右孩子为b，3表示b的左孩子为a，4表示b的右孩子为a
	TreeNode<T> *father, *child;

	if (a->left == b) flag = 1;
	if (a->right == b) flag = 2;
	if (b->left == a) flag = 3;
	if (b->right == a) flag = 4;

	father = child = nullptr;
	if (flag == 1 || flag == 2) {
		father = a; child = b;
	}
	if (flag == 3 || flag == 4) {
		father = b; child = a;
	}

	auto swapParent = [](TreeNode<T> *&x, TreeNode<T> *&y) {
		// 没有节点的parent会是NIL，不用特判NIL
		if (x->parent->left == x && y->parent->left == y) std::swap(x->parent->left, y->parent->left);
		else if (x->parent->left == x && y->parent->right == y) std::swap(x->parent->left, y->parent->right);
		else if (x->parent->right == x && y->parent->left == y) std::swap(x->parent->right, y->parent->left);
		else std::swap(x->parent->right, y->parent->right);
		std::swap(x->parent, y->parent);
	};

	auto swapLchild = [this](TreeNode<T> *&x, TreeNode<T> *&y) {
		// 对NIL进行特判
		if ((x->left != NIL && y->left != NIL)||
			(x->left == NIL && y->left == NIL)) {
			std::swap(x->left->parent, y->left->parent);
		}
		else if (x->left == NIL) {
			y->left->parent = x;
		}
		else {
			x->left->parent = y;
		}
		std::swap(x->left, y->left);
	};

	auto swapRchild = [this](TreeNode<T> *&x, TreeNode<T> *&y) {
		// 对NIL进行特判
		if ((x->right != NIL && y->right != NIL) ||
			(x->right == NIL && y->right == NIL)) {
			std::swap(x->right->parent, y->right->parent);
		}
		else if (x->right == NIL) {
			y->right->parent = x;
		}
		else {
			x->right->parent = y;
		}
		std::swap(x->right, y->right);
	};

	if (flag == 0) {
		// 处理父节点与当前节点的关系
		swapParent(a, b);
		// 处理左孩子节点与当前节点的关系
		swapLchild(a, b);
		// 处理右孩子节点与当前节点的关系
		swapRchild(a, b);
	}
	else if (flag == 1 || flag == 3) {
		assert(father != nullptr&&child != nullptr);
		//可以直接swap的是自己和右节点的关系
		swapRchild(father, child);
		//然后处理自己和父节点的关系（交换完以后a->left和a->parent会指向自己）
		swapParent(father, child);
		//最后左节点的关系只能通过赋值完成
		father->left = child->left;
		father->left->parent = father;
		child->left = father;
		child->left->parent = child;
	}
	else if (flag == 2 || flag == 4) {
		assert(father != nullptr&&child != nullptr);
		//可以直接swap的是自己和左节点的关系
		swapLchild(father, child);
		//然后处理自己和父节点的关系（交换完以后a->left和a->parent会指向自己）
		swapParent(father, child);
		//最后左节点的关系只能通过赋值完成
		father->right = child->right;
		father->right->parent = father;
		child->right = father;
		child->right->parent = child;
	}
	std::swap(a, b);
}

//TODO：插入删除过程都要维护parent
template<typename T>
TreeNode<T>* rbtree<T>::insertHelper(const T& key, void *value, TreeNode<T> *cur, TreeNode<T> *&newNode) {
	if (cur == NIL) {
		treeSize++;
		return newNode = getNewNode(key, value);
	}

	if (key < cur->key) {
		cur->left = insertHelper(key, value, cur->left, newNode);
		cur->left->parent = cur;
	}
	else {
		cur->right = insertHelper(key, value, cur->right, newNode);
		cur->right->parent = cur;
	}
	// 回溯过程中调整节点颜色
	return insertMaintain(cur);
}

template<typename T>
TreeNode<T>* rbtree<T>::eraseHelper(T& key, TreeNode<T> *cur) {
	// 查找并删除节点，为了避免维护待删除节点的的父节点，我们直接使用return递归的方式进行赋值
	//	当节点删除的时候只要返回就可以了
	if (cur == NIL) {
		return NIL;
	}
	else if (key < cur->key) {
		cur->left = eraseHelper(key, cur->left);
		cur->left->parent = cur;
	}
	else if (key > cur->key) {
		cur->right = eraseHelper(key, cur->right);
		cur->right->parent = cur;
	}
	else {
		// 已经找到待删除节点，然后我们需要分情况讨论
		TreeNode<T>* tmp = cur;
		// 如果被删除节点为红色节点，那其实路径的黑色节点数量肯定不变，但如果删除的
		//	是黑色节点，则需要把被删除节点颜色加到儿子节点上。因为，路径上的黑色数量
		//	要相同，调整是站在父节点角度来调的，所以我们只需要给儿子节点加一层被删节点
		//	的颜色就OK了
		//
		 //最终一定会在左空或者右空的分支上删除节点，此时对应儿子节点数量为1的情况
		if (cur->left == NIL) {
			cur->right->color += cur->color;
			cur = cur->right;
			treeSize--;
			delete tmp;
			return cur;
		}
		else if (cur->right == NIL) {
			cur->left->color += cur->color;
			cur = cur->left;
			treeSize--;
			delete tmp;
			return cur;
		}
		else {
			// 两边都还有节点，那就递归
			TreeNode<T>* rightMinNode = cur->right;
			while (rightMinNode->left != NIL) {
				rightMinNode = rightMinNode->left;
			}
			// 交换节点的key和value（直接交换节点的位置，里面的内容不变）
			swapNode(cur, rightMinNode);
			// 递归删除
			cur->right = eraseHelper(rightMinNode->key, cur->right);
			cur->right->parent = cur;
		}
	}
	//回溯过程中调整节点颜色
	return deleteMaintain(cur);
}

template<typename T>
TreeNode<T>* rbtree<T>::getHelper(const T& key, TreeNode<T> *cur) {
	// 查找节点
	if (cur == NIL) return NIL;

	if (key == cur->key) {
		return cur;
	}
	else if (key < cur->key) {
		return getHelper(key, cur->left);
	}
	else {
		return getHelper(key, cur->right);
	}
}

template<typename T>
void rbtree<T>::printNodesHelper(TreeNode<T> *cur) {
	if (cur == NIL) return;
	printf("(%d| %d, %d, %d, %d)\n", cur->color, cur->key, cur->left->key, cur->right->key, cur->parent->key);
	printNodesHelper(cur->left);
	printNodesHelper(cur->right);
}

template<typename T>
void rbtree<T>::clearHelper(TreeNode<T> *cur) {
	if (cur == NIL) return;
	clearHelper(cur->left);
	clearHelper(cur->right);
	delete(cur);
}

template<typename T>
TreeNode<T>* rbtree<T>::leftRotate(TreeNode<T> *cur) {
	//左旋：将右子节点的左子树变成当前节点的右子树，然后将当前节点变为右子节点的左儿子
	TreeNode<T>* originRight = cur->right;
	TreeNode<T>* originParent = cur->parent;
	//转边
	cur->right = originRight->left;
	originRight->left->parent = cur;
	//旋转维护parent
	if (originParent->left == cur) {
		originParent->left = originRight;
	}
	else {
		originParent->right = originRight;
	}
	originRight->parent = originParent;
	//旋转维护cur
	originRight->left = cur;
	cur->parent = originRight;

	return originRight;
}

template<typename T>
TreeNode<T>* rbtree<T>::rightRotate(TreeNode<T> *cur) {
	//右旋：将左子节点的右子树变成当前节点的左子树，然后将当前节点变成左子节点的右儿子
	TreeNode<T>* originLeft = cur->left;
	TreeNode<T>* originParent = cur->parent;
	//转边
	cur->left = originLeft->right;
	originLeft->right->parent = cur;
	//旋转维护parent
	if (originParent->left == cur) {
		originParent->left = originLeft;
	}
	else {
		originParent->right = originLeft;
	}
	originLeft->parent = originParent;
	//旋转维护cur
	originLeft->right = cur;
	cur->parent = originLeft;

	return originLeft;
}

template<typename T>
bool rbtree<T>::hasRedChild(TreeNode<T> *cur) {
	if (cur->left != NIL && cur->left->color == 0) return true;
	if (cur->right != NIL && cur->right->color == 0) return true;
	return false;
}

template<typename T>
TreeNode<T>* rbtree<T>::insertMaintain(TreeNode<T> *cur) {
	// 节点的维护一定是站在祖父节点的，所以我们传入的cur节点就是祖父节点
	// 由于插入的唯一的冲突为：父亲节点和子节点同为红色。为了消除这种情况
	// 我们可以分成两种情况：
	//	（1）uncle节点也为红色，那就换帽（黑红红、红黑黑）;
	//	（2）uncle节点为黑色，那就要分4种情况讨论:
	//		LL：直接大右旋（祖父节点+父亲节点），同时把升级为祖父的父亲节点所形成的塔尖变为黑红红或红黑黑；
	//		LR：先小左旋（父亲节点+红色儿子节点），此时红色儿子节点升级为父亲节点。然后再将（祖父节点+升级后的
	//			红色儿子节点）进行大右旋，此时升级后的红色儿子节点再次升级变成祖父节点。最后把升级为祖父的红色
	//			儿子节点所形成的的塔尖变成黑红红或红黑黑;
	//		RL（LR的镜像）：先小右旋，再大左旋，盖帽;
	//		RR（LL的镜像）：直接大左旋，盖帽;
	// PS：这里的盖帽统一为“红黑黑”
	if (!hasRedChild(cur)) return cur;
	// 只要有红色的父亲或叔叔节点，我们就直接盖帽（懒得再去判断有没有冲突，设置颜色不会耗费多少资源）
	if (cur->left->color == 0 && cur->right->color == 0) {
		cur->color = 0;
		cur->left->color = cur->right->color = 1;
	}
	// 剩下的就是情况（2）的冲突子情况判断了，这里如果有冲突则一定只有一边
	//	会冲突，因为你只插入了一个节点，只会影响一条路径上父节点
	int flag = 0;
	if (cur->left->color == 0 && hasRedChild(cur->left)) flag = 1;
	if (cur->right->color == 0 && hasRedChild(cur->right)) flag = 2;
	if (flag == 1) {
		// 判断是LL还是LR
		if (cur->left->right != NIL && cur->left->right->color == 0) {
			// LR先进行小左旋（这里必须刷新cur->left，因为cur->left在旋转后一定会发生变化）
			cur->left = leftRotate(cur->left);
		}
		// 大右旋（这里必须刷新cur，因为cur在旋转后一定会发生变化）
		cur = rightRotate(cur);
		// 盖帽
		cur->color = 0;
		cur->left->color = cur->right->color = 1;
	}
	else if (flag == 2) {
		// 判断是RR还是RL
		if (cur->right->left != NIL && cur->right->left->color == 0) {
			//RL先进行小右旋（这里必须刷新cur->right，因为cur->right在旋转后一定会发生变化）
			cur->right = rightRotate(cur->right);
		}
		//大左旋（这里必须刷新cur，因为cur在旋转后一定会发生变化）
		cur = leftRotate(cur);
		cur->color = 0;
		cur->left->color = cur->right->color = 1;
	}
	return cur;
}

template<typename T>
TreeNode<T>* rbtree<T>::deleteMaintain(TreeNode<T> *cur) {
	// 节点的维护一定是站在父节点（当前节点），能够进入该分支的必定是【被删节点有两个儿子节点】的情况。
	//	目前处于回溯阶段，我们需要判断的是删除节点后，两个儿子的颜色有没有双重黑，如果有，
	//	那么进一步判断他的兄弟节点是什么颜色，进而进入对应的维护分支：
	//	（1）儿子双黑，二儿子红色：【当前节点】往双黑的一边侧旋，然后【新根】改黑和【原根】改红，
	//		并往双黑节点的一边递归（因为双黑节点往下跑了一层）
	//	（2）儿子双黑，二儿子黑色：这里又要进一步分类讨论
	//		i. 若二儿子的两个儿子都是黑色：则让儿子节点和二儿子节点各减一重黑，当前节点加一重黑；
	//		ii. 若二儿子是黑色的，进入（3）；
	//	（3）儿子双黑，二儿子的两个儿子有红色节点：则优先判断二儿子和其儿子节点的关系，能否在RR或者LL组成【黑红】的情况：
	//		RR：对【当前节点】进行大左旋，然后让【新根】（原二儿子节点）变成【原父节点】的颜色，
	//			让【新根的两个儿子】（原父节点）和（原二儿子的同侧孩子节点）也变黑，最后让双黑节点减一重黑
	//		LL：对【当前节点】进行大右旋，然后让【新根】（原二儿子节点）变成【原父节点】的颜色，
	//			让【新根的两个儿子】（原父节点）和（原二儿子的同侧孩子节点）也变黑，最后让双黑节点减一重黑
	//		RL：让【二儿子】右旋，然后让【新根】变黑，【旧根】变红
	//		LR：让【二儿子】左旋，然后让【新根】变黑，【旧根】变红
	//
	//	PS：上面的所有情况最终一定会归约为RR或LL的情况
	//
	// 儿子节点里面没有双黑节点就直接返回
	if (cur->left->color != 2 && cur->right->color != 2) return cur;
	// 分支1：儿子双黑，二儿子红色
	if (hasRedChild(cur)) {
		int flag = 0;
		cur->color = 0;
		if (cur->left->color == 2) {
			cur = leftRotate(cur);
			flag = 1;
		}
		else {
			cur = rightRotate(cur);
			flag = 2;
		}
		cur->color = 1;
		if (flag == 1) {
			cur->left = deleteMaintain(cur->left);
		}
		else {
			cur->right = deleteMaintain(cur->right);
		}
		return cur;
	}
	// 分支2：儿子双黑，二儿子黑色，且二儿子两个儿子也黑
	if (cur->left->color == 1 && !hasRedChild(cur->left) ||
		cur->right->color == 1 && !hasRedChild(cur->right)) {
		cur->left->color -= 1;
		cur->right->color -= 1;
		cur->color += 1;
		return cur;
	}
	// 分支3：儿子双黑，二儿子的两个儿子有红色节点
	if (cur->left->color == 1) {
		// LR
		cur->right->color -= 1; // 双重黑节点减一重黑
		if (cur->left->left->color != 0) { // 注意：只能用!=0来判断，因为你无法确保LL节点是不是双重黑
			cur->left->color = 0;
			cur->left = leftRotate(cur->left);
			cur->left->color = 1;
		}
		// LL：大右旋，新根变原父节点颜色
		cur = rightRotate(cur);
		cur->color = cur->right->color;
	}
	else {
		// RL
		cur->left->color -= 1; // 双重黑节点减一重黑
		if (cur->right->right->color != 0) { // 注意：只能用!=0来判断，因为你无法确保RR节点是不是双重黑
			cur->right->color = 0;
			cur->right = rightRotate(cur->right);
			cur->right->color = 1;
		}
		// RR：大左旋，新根变原父节点颜色
		cur = leftRotate(cur);
		cur->color = cur->left->color;
	}
	// 左右儿子都变成黑色
	cur->left->color = cur->right->color = 1;
	return cur;
}

/*=============================================== public 函数 ===============================================*/

template<typename T>
bool rbtree<T>::insert(const T& key, void *value, TreeNode<T> *&newNode) {
	TreeNode<T> *cur = get(key);
	if (cur != nullptr && root!=NIL) return false;
	else {
		root = insertHelper(key, value, this->root, newNode);
		root->color = 1; // 根节点一定要为黑色
		return true;
	}
}

template<typename T>
bool rbtree<T>::erase(T& key) {
	TreeNode<T> *cur = get(key);
	if (cur == nullptr) return false;
	else {
		root = eraseHelper(key, this->root);
		root->color = 1;
		if (treeSize == 0) root = NIL;
		return true;
	}
}

template<typename T>
TreeNode<T>* rbtree<T>::get(const T& key) {
	if (root == NIL) return nullptr;
	TreeNode<T>* cur = getHelper(key, this->root);
	if (cur == NIL) return nullptr;
	return cur;
}

template<typename T>
TreeNode<T>* rbtree<T>::getMin() {
	if (root == NIL) return nullptr;
	TreeNode<T> *cur = root;
	while (cur->left != NIL) {
		cur = cur->left;
	}
	return cur;
}

template<typename T>
TreeNode<T>* rbtree<T>::getMax() {
	if (root == NIL) return nullptr;
	TreeNode<T> *cur = root;
	while (cur->right != NIL) {
		cur = cur->right;
	}
	return cur;
}

template<typename T>
TreeNode<T>* rbtree<T>::pop(const T& key) {
	// 查找节点
	if (root == NIL) return nullptr;
	TreeNode<T> *cur = get(key);
	if (cur == nullptr) return nullptr;
	// 拷贝节点
	TreeNode<T> *target = new TreeNode<T>(*cur);
	// 删除节点
	erase(target->key);
	return target;
}

template<typename T>
TreeNode<T>* rbtree<T>::popMin() {
	if (root == NIL) return nullptr;
	TreeNode<T>* cur = new TreeNode<T>(*getMin());
	if (cur != NIL) erase(cur->key);
	return cur;
}

template<typename T>
TreeNode<T>* rbtree<T>::popMax() {
	if (root == NIL) return nullptr;
	TreeNode<T>* cur = new TreeNode<T>(*getMax());
	if (cur != NIL) erase(cur->key);
	return cur;
}

template<typename T>
void rbtree<T>::printTree() {
	if (root == NIL) {
		std::cout << "The tree is empty." << std::endl;
		return;
	}
	std::queue<TreeNode<T>*> q;
	q.push(this->root);
	while (!q.empty()) {
		int n = q.size();
		for (int i = 0; i < n; i++) {
			auto cur = q.front();
			q.pop();
			if (cur != NIL) {
				std::cout << cur->key << " ";
				q.push(cur->left);
				q.push(cur->right);
			}
		}
		if(!q.empty()) std::cout << std::endl;
	}
}

template<typename T>
void rbtree<T>::printAllNodes() {
	if (root == NIL) {
		std::cout << "The tree is empty." << std::endl << std::endl;
		return;
	}
	printNodesHelper(root);
}

template<typename T>
int rbtree<T>::size() {
	return treeSize;
}

template<typename T>
void rbtree<T>::clear() {
	if (root == NIL) return;
	clearHelper(root);
	root = NIL;
}