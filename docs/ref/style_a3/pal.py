"""pal.py: the A2 ramps. Every ramp runs outline > shadow > base > light > highlight; shadows lean cool (toward violet /
blue), lights lean warm (toward yellow), so the ramps are hue-shifted, not just darkened. The outline of each material is
its own darkest step (coloured outlines), and the world's own darks (#453035 timber, #4D2D31 roof shadow) set how deep
they go. Sampled world colours the ramps were tuned against:
  grass #8C8D50 / #708140 / #606E37   dirt #7B5F42 / #917652   terracotta roof #B76B49 / #D8A675 / #F1A874
  green roof #44655A / #5D7B6A / #769076   timber #643C2C / #453035   slate blue #47597D / #556A90
"""

HERO = {
    # ink (soles, the darkest shared line) and face
    'K': '#21152A', '#': '#24142A', '@': '#F6EEE3', 'E': '#3A78B0', 'G': '#8FC6E8', 'M': '#8E3A40', 'Y': '#F0987E',
    # hair: copper-auburn (shadow toward plum, light toward gold)
    'a': '#3A1622', 'b': '#742E2C', 'c': '#AA4A2C', 'd': '#D9733A', 'e': '#F5B060',
    # skin
    'q': '#5E2A36', 'r': '#B8644F', 's': '#E9A077', 't': '#F8C597', 'u': '#FFE6C2',
    # tunic: teal (shadow toward indigo, light toward mint)
    '5': '#162540', '6': '#1F4A68', '7': '#28728C', '8': '#3E9FA7', '9': '#8BD8C6',
    # scarf: red (shadow toward wine, light toward orange)
    'm': '#44122C', 'n': '#87232F', 'o': '#C83E38', 'p': '#F47650',
    # leather: belt, bracers, boots, scabbard
    'f': '#2C1621', 'g': '#5B3129', 'h': '#8D5536', 'i': '#BF8249', 'j': '#E7B672',
    # trousers: slate indigo
    '1': '#1C1732', '2': '#33304E', '3': '#4F4B6B', '4': '#777493',
    # steel
    'v': '#201C31', 'w': '#525C7A', 'x': '#95A2BC', 'y': '#D5DEEA', 'z': '#FFFFFF',
    # brass
    'N': '#704020', 'O': '#CB9034', 'P': '#F8DA80',
    # the smear arc (FX, drawn by the game over the sprite): core, body, edge
    'A': '#FFFDF4', 'B': '#C9E6F4', 'C': '#7FA8D8',
}

WOMAN = {
    'K': '#21152A', '#': '#2A1424', '@': '#F7EFE4', 'E': '#3F8A5A', 'G': '#9FDCA8', 'M': '#A0414A', 'L': '#D46470',
    'Y': '#F49A8A',
    # hair: wheat blonde (shadow toward umber, light toward pale lemon)
    'a': '#3E2418', 'b': '#8C5A2A', 'c': '#C79346', 'd': '#EBC26C', 'e': '#FFEBA8',
    # skin (a touch lighter and rosier than the hero's)
    'q': '#5E2C36', 'r': '#C4705A', 's': '#EEAA86', 't': '#FCD2AA', 'u': '#FFEAD0',
    # dress: berry magenta (shadow toward violet, light toward rose)
    '5': '#2C1030', '6': '#5E1D48', '7': '#922F60', '8': '#C24F76', '9': '#EC8E9E',
    # chemise: linen white with lilac shadow
    'v': '#4B3C55', 'w': '#ADA4C0', 'x': '#DCD6E2', 'y': '#FAF6EE',
    # apron: unbleached cream with warm-grey shadow
    'A': '#4E3A30', 'B': '#A99070', 'C': '#D9C69E', 'D': '#F3E6C4',
    # shoes / bodice lacing: leather
    'f': '#2C1621', 'g': '#5B3129', 'h': '#8D5536', 'i': '#BF8249',
    # hair ribbon: teal (rhymes with the hero's tunic)
    'R': '#1F4A68', 'S': '#3E9FA7',
}

RAMPS_HERO = [('hair', 'abcde'), ('skin', 'qrstu'), ('tunic', '56789'), ('scarf', 'mnop'), ('leather', 'fghij'),
              ('trousers', '1234'), ('steel', 'vwxyz'), ('brass', 'NOP'), ('face', '#@EGMY'), ('arc FX', 'CBA')]
RAMPS_WOMAN = [('hair', 'abcde'), ('skin', 'qrstu'), ('dress', '56789'), ('chemise', 'vwxy'), ('apron', 'ABCD'),
               ('leather', 'fghi'), ('ribbon', 'RS'), ('face', '#@EGMLY')]
