def fsplit(data,sep, length=2, filler=None):
    "like split, but always return the same length. usefull to avoid assignments error"
    res = data.split(sep,length-1)
    ext = [filler]*(length-len(res))
    res.extend(ext)
    return res
