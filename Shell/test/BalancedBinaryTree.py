from typing import Union


class BalancedBinaryTreeNode:
    def __init__(self, value: int = 0):
        self.val: int = value
        self.left: Union[BalancedBinaryTreeNode, None] = None
        self.right: Union[BalancedBinaryTreeNode, None] = None
        self.parent: Union[BalancedBinaryTreeNode, None] = None
        self.balance_val: int = 0


class BalancedBinaryTree:
    def __init__(self, root: BalancedBinaryTreeNode = None):
        self.root: BalancedBinaryTreeNode = root

    def add(self, value: int) -> None:
        node = BalancedBinaryTreeNode(value)
        if not self.root:
            self.root = node
            return

        cur_node = self.search(node.val)
        if cur_node.val == node.val:
            raise RuntimeWarning('this tree has the same value of %d' % cur_node.val)
        elif cur_node.val < node.val:
            cur_node.right = node
            cur_node.balance_val += 1
            node.parent = cur_node
        else:
            cur_node.left = node
            cur_node.balance_val -= 1
            node.parent = cur_node

        cur_node = self.check_balance(cur_node.parent, cur_node)
        self.balance_tree(cur_node)

    def balance_tree(self, node: BalancedBinaryTreeNode) -> None:
        if not node:
            return
        elif node.balance_val == -2 and node.left.balance_val == -1:
            self.__ll_tree(node)
        elif node.balance_val == -2 and node.left.balance_val == 1:
            self.__lr_tree(node)
        elif node.balance_val == 2 and node.right.balance_val == 1:
            self.__rr_tree(node)
        elif node.balance_val == 2 and node.right.balance_val == -1:
            self.__rl_tree(node)

    def __ll_tree(self, node: BalancedBinaryTreeNode):
        left = node.left
        if node.parent:
            if node.parent.left == node:
                node.parent.left = left
            else:
                node.parent.right = left
        left.parent, left.right, node.left = node.parent, node, left.right
        left.right.parent = left
        left.balance_val, node.balance_val = 0, 0
        if node.left:
            node.left.parent = node
        if node == self.root:
            self.root = left

    def __rr_tree(self, node: BalancedBinaryTreeNode):
        right = node.right
        if node.parent:
            if node.parent.left == node:
                node.parent.left = right
            else:
                node.parent.right = right
        right.parent, right.left, node.right = node.parent, node, right.left
        right.left.parent = right
        right.balance_val, node.balance_val = 0, 0
        if node.right:
            node.right.parent = node
        if node == self.root:
            self.root = right

    def __lr_tree(self, node: BalancedBinaryTreeNode):
        left = node.left
        left_right = left.right
        left.right, left_right.left, node.left = left_right.left, left, left_right
        left.parent, left_right.parent = left_right, node
        left.balance_val, left_right.balance_val = 0, -1
        if left.right:
            left.right.parent = left
        self.__ll_tree(node)

    def __rl_tree(self, node: BalancedBinaryTreeNode):
        right = node.right
        right_left = right.left
        right.left, right_left.right, node.right = right_left.right, right, right_left
        right.parent, right_left.parent = right_left, node
        right.balance_val, right_left.balance_val = 0, 1
        if right.left:
            right.left.parent = right
        self.__rr_tree(node)

    @staticmethod
    def check_balance(node: BalancedBinaryTreeNode, child_node: BalancedBinaryTreeNode) \
            -> Union[BalancedBinaryTreeNode, None]:
        while node:
            if node.balance_val == 0:
                node.balance_val = node.balance_val - 1 if node.left == child_node else node.balance_val + 1
                node, child_node = node.parent, node
            else:
                node.balance_val = node.balance_val - 1 if node.left == child_node else node.balance_val + 1
                break
        else:
            return None

        return None if -1 <= node.balance_val <= 1 else node

    def search(self, value: int) -> Union[BalancedBinaryTreeNode, None]:
        if not self.root:
            return None

        cur_node = self.root
        while True:
            if cur_node.val == value:
                return cur_node

            elif cur_node.val < value:
                if not cur_node.right:
                    return cur_node

                cur_node = cur_node.right

            else:
                if not cur_node.left:
                    return cur_node

                cur_node = cur_node.left


if __name__ == '__main__':
    tree = BalancedBinaryTree()
    tree.add(20)
    tree.add(50)
    tree.add(100)
    tree.add(200)
    tree.add(150)
    tree.add(10)
    tree.add(0)
    tree.add(15)
    tree.add(19)
    print('ok')
