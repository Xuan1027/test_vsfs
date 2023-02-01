# vsfs
very simple file system
|         | superblock | inode bitmap | inode region | data bitmap | data region |
| ------- | ---------- | ------------ | ------------ | ----------- | ----------- |
| LBA     | 0          | 1            | 2~2049       | 2050+m      | 2050+m+n    | 
| blk_num | 1          | 1            | 2048         |             |             |
>blk_size=4KB

>example: total 1G, data bitmap use 8 blk, data region use 260086 blk

## struct inode
> sizeof(struct inode) = 256

|   member   | type           | note                              | size |
|:----------:| -------------- | --------------------------------- | ---- |
|    mode    | unsigned short | drwx                              | 2    |
| size/entry | unsigned int   | file max size 4GB / dir entry num | 4    |
|   atime    | time_t         | last access                       | 8    |
|   ctime    | time_t         | create                            | 8    |
|   mtime    | time_t         | modify                            | 8    |
|   blocks   | unsigned short |                                   | 2    |
| block[56]  | unsigned int*  | LBA pointer                       | 224  |

### file block[56]

| index | type    | mapping size        |
|:-----:| ------- | ------------------- |
| 0~49  | 1 level | 50x4K=20KB          |
| 50~54 | 2 level | 5x1024x4KB=20MB     |
|  55   | 3 level | 1x1024x1024x4KB=4GB |

### dir block[56]
> sizeof(entry) = 256
 
|  entry   | type           | size |
|:--------:| -------------- | ---- |
| filename | char[254]      | 254  |
| inodeptr | unsigned short | 2    |


All 1 level $\to$ 56x16=896 entries
(or the same as a normal document)
