import numpy as np

def jacobi(A, b, x0, tol=1e-6, max_iterations=100):
    n = len(b)
    x = x0.copy()
    # 提取对角矩阵 D
    D = np.diag(np.diag(A))
    # 余下部分 R = A - D
    R = A - D

    for k in range(max_iterations):
        # 迭代公式： x^(k+1) = D^(-1) * (b - R * x^(k))
        x_new = (b - np.dot(R, x)) / np.diag(A)
        # 当无穷范数收敛误差小于 tol 时退出
        if np.linalg.norm(x_new - x, ord=np.inf) < tol:
            return x_new, k+1
        x = x_new
    return x, max_iterations

# 构造 8×8 严格对角占优矩阵 A
A = np.array([
    [10, -1,  2,  0,  0,  0,  0,  0],
    [-1, 11, -1,  3,  0,  0,  0,  0],
    [2, -1, 10, -1,  0,  0,  0,  0],
    [0,  3, -1,  8,  0,  0,  0,  0],
    [0,  0,  0,  0,  9, -1,  0,  1],
    [0,  0,  0,  0, -1, 10, -1,  0],
    [0,  0,  0,  0,  0, -1,  8,  2],
    [0,  0,  0,  0,  1,  0,  2, 12]
], dtype=float)

# 右端向量 b
b = np.array([6, 25, -11, 15, 10, 12, 6, 20], dtype=float)

# 初始猜测（全 0 向量）
x0 = np.zeros(8)

# 迭代求解
solution, iterations = jacobi(A, b, x0, tol=1e-6, max_iterations=100)

print("Jacobi 迭代法在 {} 次迭代内收敛".format(iterations))
print("求得的解为：")
print(solution)

# 检查误差：计算 A*x - b
error = np.dot(A, solution) - b
print("误差：")
print(error)
