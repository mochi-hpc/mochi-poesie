-- Define the class
MyClass = {}
MyClass.__index = MyClass

-- Constructor for creating new instances of MyClass
function MyClass:new(x, y, z, t)
    local instance = setmetatable({}, self)
    instance.x = x
    instance.y = y
    instance.z = z
    instance.t = t
    return instance
end

-- Method to transform the object's attributes
function MyClass:transform(v)
    -- Increment x by v
    self.x = self.x + v

    -- Prepend "hello " to y
    self.y = "hello " .. self.y

    -- Sort the array z
    table.sort(self.z)

    -- Switch the boolean t
    self.t = not self.t
    return self
end

-- Example object
my_obj = MyClass:new(10, "world", {3, 1, 2}, true)
