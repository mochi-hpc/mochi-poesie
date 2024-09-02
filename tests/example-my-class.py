# Define the class
class MyClass:

    def __init__(self, x, y, z, t):
        self.x = x
        self.y = y
        self.z = z
        self.t = t

    def transform(self, v):
        self.x = self.x + v
        self.y = "hello " + self.y
        self.z = list(sorted(self.z))
        self.t = not self.t
        return self.__dict__

# Example object
my_obj = MyClass(10, "world", [3, 1, 2], True)
