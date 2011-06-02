import random
import itertools

import Orange

def add_results_to_items(G, lblhistory):
    attrs = [Orange.data.variable.Discrete('clustering label propagation',
                                values=list(set([l for l in lblhistory[-1]])))]
    
    dom = Orange.data.Domain(attrs, 0)
    data = Orange.data.Table(dom, [[l] for l in lblhistory[-1]])
    if G.items() is None:
        G.set_items(data) 
    else: 
        G.set_items(Orange.data.Table([G.items(), data]))
        
def add_history_to_items(G, lblhistory):
    attrs = [Orange.data.variable.Discrete('c'+ str(i), values=list(set( \
            [l for l in lblhistory[0]]))) for i,lbls in enumerate(lblhistory)]
    
    dom = Orange.data.Domain(attrs, 0)
    # transpose history
    data = map(list, zip(*lblhistory))
    data = Orange.data.Table(dom, data)
    if G.items() is None:
        G.set_items(data)  
    else: 
        G.set_items(Orange.data.Table([G.items(), data]))
            
def label_propagation_hop_attenuation(G, results2items=0, resultHistory2items=0, iterations=1000, delta=0.1, node_degree_preference=0):
    """Label propagation for community detection, Leung et al., 2009
    
    :param results2items: append a new feature result to items 
        (Orange.data.Table)
    :type results2items: bool
    :param resultHistory2items: append new features result to items 
        (Orange.data.Table) after each iteration of the algorithm
    :type resultHistory2items: bool
    """
    
    if G.is_directed():
        raise Orange.network.nx.NetworkXError("""Not allowed for directed graph
              G Use UG=G.to_undirected() to create an undirected graph.""")
    
    vertices = G.nodes()
    degrees = dict(zip(vertices, [G.degree(v) for v in vertices]))
    labels = dict(zip(vertices, range(G.number_of_nodes())))
    scores = dict(zip(vertices, [1] * G.number_of_nodes()))
    lblhistory = []
    m = node_degree_preference
    
    for i in range(iterations):
        random.shuffle(vertices)
        stop = 1
        for v in vertices:
            neighbors = G.neighbors(v)
            if len(neighbors) == 0:
                continue
            
            lbls = sorted(((G.edge[v][u].get('weight', 1), labels[u], u) for u in neighbors), key=lambda x: x[1])
            lbls = [(sum(scores[u] * degrees[u]**m * weight for weight, u_label, u in group), label) for label, group in itertools.groupby(lbls, lambda x: x[1])]
            max_score = max(lbls)[0]
            max_lbls = [label for score, label in lbls if score >= max_score]
            
            # only change label if it is not already one of the 
            # preferred (maximal) labels
            if labels[v] not in max_lbls:
                labels[v] = random.choice(max_lbls)
                scores[v] = max(0, max(scores[u] for u in neighbors if labels[u] == labels[v]) - delta)
                stop = 0
            
        lblhistory.append([str(labels[key]) for key in sorted(labels.keys())])
        # if stopping condition is satisfied (none of the labels switched color)
        if stop:
            break
            
    if results2items and not resultHistory2items:
        add_results_to_items(G, lblhistory)
        
    if resultHistory2items:
        add_history_to_items(G, lblhistory)
    
    print "iterations:", i
    return labels

def label_propagation(G, results2items=0, resultHistory2items=0, iterations=1000):
    """Label propagation for community detection, Raghavan et al., 2007
    
    :param results2items: append a new feature result to items 
        (Orange.data.Table)
    :type results2items: bool
    :param resultHistory2items: append new features result to items 
        (Orange.data.Table) after each iteration of the algorithm
    :type resultHistory2items: bool
    """
    
    def next_label(neighbors):
        """Updating rule as described by Raghavan et al., 2007
        
        Return a list of possible node labels with equal probability.
        """
        lbls = sorted(labels[u] for u in neighbors)
        lbls = [(len(list(c)), l) for l, c in itertools.groupby(lbls)]
        m = max(lbls)[0]
        return [l for c, l in lbls if c >= m]
    
    vertices = G.nodes()
    labels = dict(zip(vertices, range(G.number_of_nodes()))) 
    lblhistory = []
    for i in range(iterations):
        random.shuffle(vertices)
        stop = 1
        for v in vertices:
            nbh = G.neighbors(v)
            if len(nbh) == 0:
                continue
            
            max_lbls = next_label(nbh)
            
            if labels[v] not in max_lbls:
                stop = 0
            
            labels[v] = random.choice(max_lbls)
            
        lblhistory.append([str(labels[key]) for key in sorted(labels.keys())])
        # if stopping condition might be satisfied, check it
        # stop when no label would switch anymore
        if stop:
            for v in vertices:
                nbh = G.neighbors(v)
                if len(nbh) == 0: 
                    continue
                max_lbls = next_label(nbh)
                if labels[v] not in max_lbls: 
                    stop = 0
                    break
            
            if stop: break
                
    if results2items and not resultHistory2items:
        add_results_to_items(G, lblhistory)
        
    if resultHistory2items:
        add_history_to_items(G, lblhistory)
        
    print "iterations:", i
    return labels
